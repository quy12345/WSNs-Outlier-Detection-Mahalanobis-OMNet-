# Báo cáo Dự án: Tái hiện Thuật toán ODA-MD trên OMNeT++

**Thuật toán Phát hiện Dữ liệu Ngoại lai dựa trên Khoảng cách Mahalanobis cho Mạng Cảm biến Không dây**

---

## 1. Giới thiệu

### 1.1 Vấn đề

Trong Mạng Cảm biến Không dây (WSN), dữ liệu thu thập thường bị nhiễu bởi:
- **Lỗi cảm biến** (sensor malfunction): Do pin yếu, hư hỏng phần cứng
- **Nhiễu môi trường**: Giao thoa sóng, thời tiết khắc nghiệt
- **Sự kiện thực (Events)**: Cháy rừng, động đất - đây là dữ liệu quan trọng cần giữ lại!

**Thách thức**: Làm sao phân biệt giữa **lỗi cảm biến** (cần loại bỏ) và **sự kiện thực** (cần giữ lại)?

### 1.2 Giải pháp

Bài báo của Titouna et al. (ICCCI-2019) đề xuất **ODA-MD** - thuật toán phát hiện outlier tại Cluster Head, tận dụng:
- **Spatial Correlation**: Các sensor gần nhau đo giá trị tương tự
- **Mahalanobis Distance**: Đo "độ bất thường" có xét đến tương quan giữa các biến

---

## 2. Cơ sở Lý thuyết

### 2.1 Khoảng cách Mahalanobis (MD)

**Định nghĩa**: Đo khoảng cách từ một điểm đến phân phối dữ liệu, có tính đến correlation giữa các biến.

$$MD_i = \sqrt{(X_i - \mu)^T \Sigma^{-1} (X_i - \mu)}$$

| Thành phần | Ý nghĩa | Ví dụ |
|------------|---------|-------|
| $X_i$ | Vector quan sát | (23°C, 40%, 500lux, 2.5V) |
| $\mu$ | Vector trung bình | (22°C, 38%, 480lux, 2.4V) |
| $\Sigma$ | Ma trận hiệp phương sai | Thể hiện correlation giữa T,H,L,V |
| $\Sigma^{-1}$ | Nghịch đảo của $\Sigma$ | "Chuẩn hóa" theo correlation |

### 2.2 Tại sao không dùng Euclidean Distance?

**Ví dụ minh họa:**

Giả sử có 3 sensor đo nhiệt độ (T) và độ ẩm (H):

| Sensor | T (°C) | H (%) | Bình thường? |
|--------|--------|-------|--------------|
| A | 25 | 60 | Bình thường |
| B | 30 | 45 | Bình thường |
| C | 25 | 45 | Bất thường |

**Euclidean**: Sensor C gần trung bình → Bình thường

**Mahalanobis**: T và H có **tương quan âm** (T tăng → H giảm). Sensor C có T thấp nhưng H cũng thấp → **BẤT THƯỜNG**!

```
     H(%)
      │
   60 ┤  A ●
      │      ╲ (correlation line)
   45 ┤        ● B     ● C (outlier!)
      │         ╲
      └──────────┴──────── T(°C)
              25    30
```

### 2.3 Ngưỡng Quyết định (Threshold)

$MD^2$ tuân theo phân phối **Chi-bình phương** với $p$ bậc tự do (p = số thuộc tính).

Với 4 thuộc tính (T, H, L, V) và mức ý nghĩa 97.5%:

$$\text{Threshold} = \sqrt{\chi^2_{4, 0.975}} = \sqrt{11.143} \approx 3.338$$

**Quy tắc**: Nếu $MD \geq 3.338$ → **Outlier**

---

## 3. Thuật toán ODA-MD (Chi tiết)

### 3.1 Kiến trúc Phân tán

```
┌─────────────────────────────────────────────────────────────┐
│                        CLUSTER                              │
│                                                             │
│   ┌─────────┐    ┌─────────┐    ┌─────────┐                │
│   │Sensor 36│    │Sensor 37│    │Sensor 38│                │
│   └────┬────┘    └────┬────┘    └────┬────┘                │
│        │              │              │                      │
│        └──────────────┼──────────────┘                      │
│                       ▼                                     │
│              ┌────────────────┐                             │
│              │  Cluster Head  │ ← Chạy ODA-MD tại đây       │
│              │   (Node 2)     │                             │
│              └───────┬────────┘                             │
│                      │                                      │
└──────────────────────┼──────────────────────────────────────┘
                       ▼
                 ┌──────────┐
                 │   SINK   │ ← Chỉ nhận dữ liệu sạch
                 └──────────┘
```

### 3.2 Các bước thuật toán

**Algorithm 1: Initialization (Khởi tạo)**

```
1. CH gửi request đến tất cả sensors trong cluster
2. Mỗi sensor đo môi trường và gửi vector (T, H, L, V) về CH
3. CH sử dụng cơ chế Sliding Window (cửa sổ trượt) với kích thước 20 mẫu
```

**Algorithm 2: MD Calculation & Outlier Detection (Sliding Window)**

```
GIAI ĐOẠN KHỞI TẠO (20 mẫu đầu tiên):
1. Thu thập đủ 20 mẫu vào sliding window
2. Tính vector trung bình μ và ma trận hiệp phương sai Σ từ 20 mẫu
3. Nghịch đảo Σ → Σ⁻¹
4. Tính MD cho TẤT CẢ 20 mẫu:
   - Nếu MD_i ≥ 3.338 → Outlier → BLOCK (không gửi về Sink, nhưng GIỮ trong window)
   - Nếu MD_i < 3.338 → Normal → CHUYỂN TIẾP về Sink

GIAI ĐOẠN SLIDING (các mẫu tiếp theo):
1. Thêm mẫu mới vào cuối window (push_back)
2. Loại bỏ mẫu cũ nhất (pop_front) để giữ kích thước = 20
3. Tính lại μ và Σ từ 20 mẫu trong window hiện tại
4. Tính MD CHỈ cho mẫu mới nhất:
   - Nếu MD ≥ 3.338 → Outlier → BLOCK (giữ trong window để phân biệt Error/Event)
   - Nếu MD < 3.338 → Normal → CHUYỂN TIẾP về Sink
```

**Ưu điểm của Sliding Window:**
-  Xử lý **real-time** (gần như tức thời sau giai đoạn khởi động)
-  Mean và Covariance luôn **"trượt" theo xu hướng dữ liệu mới nhất**
-  Giữ outlier trong window để **phân biệt Error vs Event**

### 3.3 Ví dụ Cụ thể

**Batch dữ liệu** (20 samples từ 3 sensors):

| Sample | Sensor | T(°C) | H(%) | L(lux) | V(V) |
|--------|--------|-------|------|--------|------|
| 1 | 36 | 22.5 | 38.2 | 480 | 2.45 |
| 2 | 37 | 23.1 | 37.8 | 495 | 2.48 |
| 3 | 38 | 22.8 | 38.5 | 488 | 2.46 |
| ... | ... | ... | ... | ... | ... |
| 15 | 36 | **85.0** | **8.0** | **2500** | **4.5** | ← **OUTLIER!** |
| ... | ... | ... | ... | ... | ... |

**Tính toán:**
- $\mu$ = (22.9, 38.1, 487, 2.46)
- Sample 15: MD = **12.5** >> 3.338 → **Outlier → Loại bỏ!**

**Lý do**: Sample 15 có T quá cao, H quá thấp, L quá cao, V quá cao - tất cả đều lệch khỏi pattern bình thường.

---

## 4. Cơ chế Phân biệt Error vs Event

### 4.1 ODA-MD: Phân biệt NGẦM qua Mean Shift

**Điểm thông minh của ODA-MD**: Thuật toán có khả năng **tự động phân biệt** Error và Event thông qua cơ chế tính mean!

#### Scenario 1: ERROR (Chỉ 1 sensor lỗi)

| Sample | Sensor | T(°C) | Phân tích |
|--------|--------|-------|-----------|
| 1 | 36 | 22.5 | Bình thường |
| 2 | 37 | 23.1 | Bình thường |
| 3 | 38 | **85.0** | **Lỗi sensor!** |

**Tính toán:**
- $\mu_T$ = (22.5 + 23.1 + 85.0) / 3 = **43.5°C**
- Sample 3: |85.0 - 43.5| = 41.5 → **MD rất cao** → **Outlier → Loại bỏ!** 

#### Scenario 2: EVENT (Tất cả sensors cùng tăng - ví dụ: cháy rừng)

| Sample | Sensor | T(°C) | Phân tích |
|--------|--------|-------|-----------|
| 1 | 36 | **65.0** | Event! |
| 2 | 37 | **68.0** | Event! |
| 3 | 38 | **70.0** | Event! |

**Tính toán:**
- $\mu_T$ = (65.0 + 68.0 + 70.0) / 3 = **67.7°C** ← Mean bị "nhiễm" bởi event!
- Sample 1: |65.0 - 67.7| = 2.7
- Sample 2: |68.0 - 67.7| = 0.3  
- Sample 3: |70.0 - 67.7| = 2.3
- **Tất cả MD đều THẤP** → **Không có outlier** → **Chuyển tiếp về Sink!** 

```
┌────────────────────────────────────────────────────────────────┐
│ ERROR (1 sensor lỗi)          │ EVENT (nhiều sensors)          │
├───────────────────────────────┼────────────────────────────────┤
│                               │                                │
│ Mean ở giữa, lỗi ở xa         │ Mean SHIFT theo event          │
│                               │                                │
│      ●──●         ●           │         ●──●──●                │
│      │  │         ↑           │         (mean mới)             │
│   [mean]         Outlier!     │         Không có outlier!      │
│                               │                                │
│ → MD cao → Loại bỏ            │ → MD thấp → Chuyển tiếp        │
└───────────────────────────────┴────────────────────────────────┘
```

> **Kết luận**: ODA-MD tự động phân biệt Error/Event nhờ spatial correlation - khi tất cả sensors trong cluster cùng bất thường, mean bị "kéo" theo → không ai là outlier → dữ liệu event được giữ lại!

---

## 5. Thuật toán OD Baseline (Fawzy et al., 2013)

### 5.1 Tổng quan

OD (Outlier Detection and Classification) là thuật toán được đề xuất bởi Fawzy et al. trong bài báo *"Outliers detection and classification in wireless sensor networks"* (Egyptian Informatics Journal, 2013).

**Đặc điểm chính:**
- Sử dụng **Fixed-Width Clustering** để gom nhóm dữ liệu
- Phát hiện outlier dựa trên **khoảng cách Euclidean** giữa các cluster
- Phân biệt **Error vs Event** một cách tường minh (explicit)
- Tính **Sensor Trustfulness** để đánh giá độ tin cậy

### 5.2 Các bước thuật toán OD (Chi tiết)

```
┌─────────────────────────────────────────────────────────────────────────┐
│ STEP 1: Fixed-Width Clustering                                          │
│   - Gom các điểm dữ liệu gần nhau thành clusters                        │
│   - Nếu distance(point, center) ≤ cluster_width → Thêm vào cluster      │
│   - Ngược lại → Tạo cluster mới                                         │
├─────────────────────────────────────────────────────────────────────────┤
│ STEP 2: Outlier Detection                                               │
│   - Tính khoảng cách trung bình giữa các cluster                        │
│   - Cluster có distance > mean + std → OUTLIER CLUSTER                  │
├─────────────────────────────────────────────────────────────────────────┤
│ STEP 3: Classification (Error vs Event)                                 │
│   - Kiểm tra outlier cluster chứa bao nhiêu sensors khác nhau?          │
│   - Nếu 1 sensor → ERROR (lỗi sensor) → Loại bỏ                         │
│   - Nếu ≥2 sensors → EVENT (sự kiện thực) → Giữ lại!                    │
├─────────────────────────────────────────────────────────────────────────┤
│ STEP 4: Sensor Trustfulness                                             │
│   - Trust(s_i) = 1 - (N_errors / N_total)                               │
│   - Đánh giá độ tin cậy của sensor dựa trên lịch sử lỗi                 │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.3 Ví dụ Fixed-Width Clustering

**Dữ liệu đầu vào** (20 samples, chỉ hiển thị Temperature):

| Sample | Sensor | T(°C) | Ghi chú |
|--------|--------|-------|---------|
| 1 | 36 | 22.5 | Bình thường |
| 2 | 37 | 23.1 | Bình thường |
| 3 | 38 | 22.8 | Bình thường |
| 4 | 36 | 23.0 | Bình thường |
| 5 | 37 | 22.7 | Bình thường |
| ... | ... | ... | ... |
| 18 | 38 | **85.0** | **OUTLIER!** |
| 19 | 36 | 22.9 | Bình thường |
| 20 | 37 | 23.2 | Bình thường |

**Quá trình phân cụm** (cluster_width = 50):

```
Bước 1: Sample 1 (T=22.5) → Tạo Cluster 0 (center=22.5)
Bước 2: Sample 2 (T=23.1) → |23.1-22.5|=0.6 ≤ 50 → Thêm vào Cluster 0
Bước 3: Sample 3 (T=22.8) → |22.8-22.8|=0 ≤ 50 → Thêm vào Cluster 0
...
Bước 18: Sample 18 (T=85.0) → |85.0-22.9|=62.1 > 50 → Tạo Cluster 1 (center=85.0)
Bước 19: Sample 19 (T=22.9) → |22.9-22.9|=0 ≤ 50 → Thêm vào Cluster 0
Bước 20: Sample 20 (T=23.2) → |23.2-22.9|=0.3 ≤ 50 → Thêm vào Cluster 0
```

**Kết quả:**

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Cluster 0: center=22.9°C, members=[1,2,3,4,5,...,17,19,20] (19 samples) │
│ Cluster 1: center=85.0°C, members=[18] (1 sample)          ← OUTLIER!  │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.4 Ví dụ Outlier Detection (Step 2)

**Tính khoảng cách giữa các cluster:**

```
Distance(Cluster 0, Cluster 1) = |22.9 - 85.0| = 62.1
Mean distance = 62.1 / 1 = 62.1
Std = 0 (chỉ có 1 khoảng cách)

Threshold = mean + std = 62.1 + 0 = 62.1
```

**Cluster 1** có distance = 62.1 ≥ threshold → **OUTLIER CLUSTER**

### 5.5 Ví dụ Classification (Step 3)

**Kiểm tra Cluster 1 (outlier):**
- Members: [Sample 18] 
- Sensors trong cluster: {Sensor 38}
- Số lượng sensors = 1

**Kết luận:** Chỉ 1 sensor → **ERROR** (lỗi sensor 38) → **Loại bỏ!**

**Nếu ngược lại:**
```
Cluster 1 có members: [Sample 18 (Sensor 38), Sample X (Sensor 37)]
→ Sensors: {38, 37} → 2 sensors
→ EVENT (sự kiện thực, ví dụ: cháy) → GIỮ LẠI!
```

### 5.6 Công thức Sensor Trustfulness (Step 4)

Theo paper Fawzy et al.:

$$Trust(s_i) = 1 - \frac{N_{ol}}{N_i}$$

| Ký hiệu | Ý nghĩa |
|---------|---------|
| $Trust(s_i)$ | Độ tin cậy của sensor $s_i$ (0-1) |
| $N_{ol}$ | Số lần sensor gửi dữ liệu lỗi (error) |
| $N_i$ | Tổng số lần sensor gửi dữ liệu |

**Ví dụ:**
```
Sensor 36: N_total=6000, N_errors=150
→ Trust = 1 - (150/6000) = 0.975 = 97.5% [RELIABLE]

Sensor 38: N_total=6000, N_errors=1500  
→ Trust = 1 - (1500/6000) = 0.75 = 75% [UNRELIABLE - Xem xét thay thế]
```

### 5.2 So sánh ODA-MD vs OD

| Khía cạnh | ODA-MD | OD |
|-----------|--------|-----|
| **Phân biệt Error/Event** |  Ngầm (qua mean shift) |  Tường minh (step 3) |
| **Cơ chế xử lý** | Sliding Window (real-time) | Batch Processing |
| **Số bước xử lý** | 2 bước | 4 bước |
| **Communication overhead** | Thấp | Cao (trao đổi clustering) |
| **Độ chính xác** | **99.49%** | 46.26% |
| **Tiêu thụ năng lượng** | **1123.3 mJ** | 1510.9 mJ (+34%) |

---

## 5. Triển khai trên OMNeT++

### 5.1 Tổng quan Code

| Module | File | Chức năng |
|--------|------|-----------|
| **SensorNode** | `SensorNode.cc` | Đọc Intel Lab data, gửi về CH khi nhận request |
| **ClusterHead** | `ClusterHead.cc` | Triển khai ODA-MD và OD, tính toán MD, quyết định outlier |
| **Sink** | `Sink.cc` | Nhận và đếm packets sạch |
| **IntelLabData** | `IntelLabData.h` | Load dataset, inject 1000 outliers |
| **EnergyModel** | `EnergyModel.h` | Mô hình năng lượng Heinzelman |
| **MetricsCollector** | `MetricsCollector.h` | Thu thập DA, FAR, confusion matrix |

### 5.2 Luồng xử lý (Sliding Window)

```
1. ClusterHead gửi RequestMsg mỗi 1s
2. SensorNodes đọc dữ liệu Intel Lab → trả về SensorMsg
3. ClusterHead thêm sample vào sliding window (deque)
4. Nếu window size > 20 → pop_front() (loại mẫu cũ nhất)
5. Khi window đủ 20 mẫu → runODAMD():
   - Giai đoạn khởi tạo: Tính MD cho tất cả 20 mẫu
   - Giai đoạn sliding: Chỉ tính MD cho mẫu mới nhất
6. Outliers bị block (giữ trong window), Normal data chuyển tiếp về Sink
7. MetricsCollector ghi nhận TP/FP/TN/FN
```

### 5.3 Cấu hình Mô phỏng

| Tham số | ODA-MD | OD |
|---------|--------|-----|
| Threshold | 3.338 (Chi-square) | 15.0 (cluster width) |
| **Cơ chế xử lý** | **Sliding Window** | Batch |
| Window size | 20 | 20 |
| Data source | Intel Lab (motes 36,37,38) | Intel Lab (motes 36,37,38) |
| Outliers injected | 1000 | 1000 |
| Simulation time | 6000s | 6000s |

### 5.4 Mô phỏng Nút Cảm biến MICA2

Trong mô phỏng, các nút cảm biến được mô phỏng dựa trên **MICA2 Mote** - nền tảng phần cứng phổ biến trong nghiên cứu WSN:

| Thông số | Giá trị | Mô tả |
|----------|---------|-------|
| **Vi xử lý** | ATmega128L @ 8MHz | Không có FPU (Floating Point Unit) |
| **Năng lượng khởi tạo** | 2 Joules | Theo mô hình Heinzelman |
| **Xử lý floating-point** | ~100 cycles/FLOP | Phần mềm mô phỏng FP |
| **Matrix 4×4 inversion** | ~1000 FLOPs → 12.5ms | Nghịch đảo ma trận hiệp phương sai |

**Các thông số năng lượng (Heinzelman Model):**
- `E_ELEC = 50 nJ/bit` — Năng lượng mạch điện tử TX/RX
- `E_AMP = 100 pJ/bit/m²` — Năng lượng khuếch đại truyền
- `E_DA = 5 nJ/bit` — Năng lượng tổng hợp dữ liệu

---

## 6. Kết quả Mô phỏng

### 6.1 Kết quả Chi tiết

| Metric | ODA-MD | OD | Nhận xét |
|--------|--------|-----|----------|
| **Detection Accuracy (Recall)** | **99.49%** | 46.26% | ODA-MD phát hiện 970/975 outliers |
| **Precision** | **97.39%** | 15.34% | ODA-MD: TP/(TP+FP) = 970/996 |
| **F1-Score** | **98.43%** | 23.02% | Cân bằng giữa Precision và Recall |
| **False Alarm Rate** | **0.15%** | 14.61% | OD có quá nhiều False Positives |
| **Energy (CH)** | **1123.3 mJ** | 1510.9 mJ | ODA-MD tiết kiệm 26% |
| True Positives (TP) | 970 | 451 | Outliers phát hiện đúng |
| False Positives (FP) | 26 | 2488 | OD cảnh báo sai quá nhiều |
| False Negatives (FN) | 5 | 524 | OD bỏ sót nhiều outliers |
| True Negatives (TN) | ~17000 | ~14500 | Dữ liệu bình thường chuyển tiếp đúng |

### 6.2 Công thức Tính các Chỉ số

$$\text{Precision} = \frac{TP}{TP + FP}$$

$$\text{Recall (Detection Accuracy)} = \frac{TP}{TP + FN}$$

$$\text{F1-Score} = 2 \times \frac{Precision \times Recall}{Precision + Recall}$$

$$\text{False Alarm Rate (FAR)} = \frac{FP}{FP + TN}$$

| Thuật toán | Precision | Recall | F1-Score | Giải thích |
|------------|-----------|--------|----------|------------|
| **ODA-MD** | 970/(970+26) = **97.39%** | 970/(970+5) = **99.49%** | **98.43%** | Cả hai chỉ số đều rất cao |
| **OD** | 451/(451+2488) = **15.34%** | 451/(451+524) = **46.26%** | **23.02%** | Precision quá thấp do FP cao |

### 6.3 Phân tích

**Tại sao ODA-MD vượt trội?**

1. **Mahalanobis xem xét correlation**: Phát hiện outliers "ẩn" mà Euclidean bỏ sót
2. **Threshold thống kê chặt chẽ**: Dựa trên Chi-square distribution, giảm False Alarm
3. **Sliding Window real-time**: Phát hiện outlier ngay khi nhận (sau giai đoạn khởi động)
4. **Mean "trượt" theo xu hướng mới nhất**: Tự động thích ứng với sự thay đổi môi trường

**Tại sao OD kém hơn?**

1. **Euclidean không xét correlation**: Bỏ sót outliers trong không gian đa biến
2. **Fixed-width clustering nhạy cảm**: Cluster width 50.0 có thể không phù hợp với data
3. **Overhead cao**: 4 bước xử lý + trao đổi thông tin giữa clusters → tốn năng lượng
4. **Batch processing**: Phải chờ đủ batch mới xử lý, tăng độ trễ phát hiện

[PLACEHOLDER: Biểu đồ Detection Accuracy vs Time]

[PLACEHOLDER: Biểu đồ Energy Consumption Comparison]

---

## 7. Kết luận

Dự án đã **tái hiện thành công** thuật toán ODA-MD với kết quả:

| Mục tiêu | Kết quả | Đánh giá |
|----------|---------|----------|
| DA ~95-100% | **99.49%** | Vượt mong đợi |
| FAR ~1-5% | **0.15%** | Xuất sắc |
| Energy ODA-MD < OD | **26% thấp hơn** | Đúng như bài báo |

**Cải tiến Sliding Window:**
- Giảm độ trễ phát hiện xuống gần như tức thời (sau 20 mẫu khởi động)
- Mean và Covariance luôn cập nhật theo xu hướng dữ liệu mới nhất
- Giữ outlier trong window để hỗ trợ phân biệt Error/Event

**Kết luận**: ODA-MD với cơ chế Sliding Window là giải pháp hiệu quả cho việc lọc nhiễu đa biến trong WSN, đặc biệt phù hợp với yêu cầu xử lý real-time. Thuật toán tự động phân biệt Error/Event nhờ spatial correlation và cơ chế mean shift.

---

## 8. Chi tiết Cách tính Năng lượng Tiêu thụ trong WSN

### 8.1 Mô hình Năng lượng Heinzelman

Dự án sử dụng **mô hình năng lượng Heinzelman** (Heinzelman et al., IEEE 2002) - mô hình chuẩn được sử dụng rộng rãi trong nghiên cứu WSN. Mô hình này mô phỏng chi phí năng lượng cho việc truyền, nhận và xử lý dữ liệu.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      MÔ HÌNH NĂNG LƯỢNG HEINZELMAN                      │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ┌──────────────────┐         ┌──────────────────┐                    │
│   │   TRANSMITTER    │  ─────▶ │     RECEIVER     │                    │
│   │   ┌──────────┐   │   d(m)  │   ┌──────────┐   │                    │
│   │   │ TX Elec. │   │         │   │ RX Elec. │   │                    │
│   │   │ E_elec   │   │         │   │ E_elec   │   │                    │
│   │   └──────────┘   │         │   └──────────┘   │                    │
│   │   ┌──────────┐   │         │                  │                    │
│   │   │TX Amplif.│   │         │                  │                    │
│   │   │ E_amp    │   │         │                  │                    │
│   │   └──────────┘   │         │                  │                    │
│   └──────────────────┘         └──────────────────┘                    │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 8.2 Các Hằng số Năng lượng

| Tham số | Ký hiệu | Giá trị | Ý nghĩa |
|---------|---------|---------|---------|
| **Electronics Energy** | $E_{elec}$ | 50 nJ/bit | Năng lượng mạch điện tử (TX/RX) |
| **Amplifier Energy** | $E_{amp}$ | 100 pJ/bit/m² | Năng lượng khuếch đại |
| **Data Aggregation** | $E_{DA}$ | 5 nJ/bit | Năng lượng tổng hợp dữ liệu |
| **Processing Energy** | - | 10 nJ/operation | Năng lượng tính toán CPU |
| **Threshold Distance** | $d_0$ | 87 m | Ngưỡng chuyển mô hình |

### 8.3 Công thức Tính Năng lượng

#### 8.3.1 Truyền dữ liệu (Transmit)

Năng lượng truyền $k$ bits qua khoảng cách $d$:

**Mô hình Free-Space** (khi $d < d_0$):
$$E_{TX}(k, d) = E_{elec} \times k + E_{amp} \times k \times d^2$$

**Mô hình Multipath Fading** (khi $d \geq d_0$):
$$E_{TX}(k, d) = E_{elec} \times k + E_{mp} \times k \times d^4$$

Với $E_{mp} = 0.0013 \times 10^{-12}$ J/bit/m⁴

**Ví dụ tính toán:**
```
Truyền 256 bits qua 20m (Free-Space):
E_TX = 50×10⁻⁹ × 256 + 100×10⁻¹² × 256 × 20²
     = 12.8 µJ + 10.24 µJ
     = 23.04 µJ = 0.023 mJ
```

#### 8.3.2 Nhận dữ liệu (Receive)

Năng lượng nhận $k$ bits:
$$E_{RX}(k) = E_{elec} \times k$$

**Ví dụ:**
```
Nhận 256 bits:
E_RX = 50×10⁻⁹ × 256 = 12.8 µJ = 0.0128 mJ
```

#### 8.3.3 Xử lý CPU (Processing)

Năng lượng xử lý dựa trên số operations:
$$E_{CPU} = 10 \times 10^{-9} \times N_{ops}$$

| Tác vụ | Số Operations | Năng lượng |
|--------|---------------|------------|
| Ma trận 4×4 inversion | ~1000 FLOPs | 10 µJ |
| Fixed-width clustering | ~200 FLOPs | 2 µJ |
| Trust calculation | ~50 FLOPs/sensor | 0.5 µJ |

### 8.4 Chi tiết Tiêu thụ Năng lượng theo Module

#### 8.4.1 SensorNode

```c++
// Năng lượng khởi tạo: 2 Joules
energy = EnergyModel(2.0);

// Khi nhận request từ CH (64 bits)
energy.receive(64);  // 3.2 µJ

// Khi gửi data về CH (256 bits, 20m)
energy.transmit(256, 20.0);  // 23.04 µJ
```

**Tổng mỗi chu kỳ request-response:**
$$E_{Sensor/cycle} = 3.2 + 23.04 = 26.24 \text{ µJ}$$

#### 8.4.2 ClusterHead - ODA-MD

```c++
// Năng lượng khởi tạo: 5 Joules (cao hơn sensor)
energy = EnergyModel(5.0);

// Gửi request (64 bits × numSensors, 20m)
energy.transmit(64, 20.0);  // Per sensor

// Nhận data từ sensors (256 bits)
energy.receive(256);

// Xử lý ODA-MD (matrix inversion ~1000 FLOPs)
energy.process(1000);

// Chuyển tiếp data sạch về Sink (256 bits, 30m)
energy.transmit(256, 30.0);
```

**Năng lượng mỗi batch ODA-MD:**

| Hoạt động | Công thức | Năng lượng |
|-----------|-----------|------------|
| Gửi 3 requests | $3 \times E_{TX}(64, 20)$ | ~17.6 µJ |
| Nhận 3 responses | $3 \times E_{RX}(256)$ | ~38.4 µJ |
| Matrix computation | $E_{CPU}(1000)$ | 10 µJ |
| Forward to Sink | $E_{TX}(256, 30)$ × clean_samples | ~variable |

#### 8.4.3 ClusterHead - OD Algorithm

OD Algorithm có **overhead năng lượng cao hơn** do các bước bổ sung:

```c++
// STEP 1: Fixed-Width Clustering - Cần trao đổi thông tin giữa sensors
energy.transmit(128 * numDataPoints, 30.0);  // Broadcast data
energy.receive(64 * numDataPoints);           // Receive assignments

// STEP 2: Inter-cluster Distance - Trao đổi giữa các Cluster Heads
energy.transmit(128 * numClusters, 50.0);    // Send center info (xa hơn)
energy.receive(128 * numClusters * (numClusters - 1));

// STEP 3: Classification - Query neighbors
energy.transmit(64 * numOutlierClusters * numSensors, 30.0);
energy.receive(64 * numOutlierClusters * numSensors);
```

### 8.5 So sánh Chi tiết Năng lượng ODA-MD vs OD

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    SO SÁNH NĂNG LƯỢNG TIÊU THỤ                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ODA-MD (1123.3 mJ)              OD (1510.9 mJ)                        │
│  ═══════════════════             ═══════════════════                   │
│                                                                         │
│  ┌─────────────────┐             ┌─────────────────┐                   │
│  │ Request-Receive │             │ Request-Receive │                   │
│  │     ~40%        │             │     ~30%        │                   │
│  ├─────────────────┤             ├─────────────────┤                   │
│  │ MD Calculation  │             │ Clustering      │                   │
│  │     ~15%        │             │     ~20%        │ ← OVERHEAD        │
│  ├─────────────────┤             ├─────────────────┤                   │
│  │ Forward to Sink │             │ Detection       │                   │
│  │     ~45%        │             │     ~15%        │ ← OVERHEAD        │
│  └─────────────────┘             ├─────────────────┤                   │
│                                  │ Classification  │                   │
│                                  │     ~10%        │ ← OVERHEAD        │
│                                  ├─────────────────┤                   │
│                                  │ Forward to Sink │                   │
│                                  │     ~25%        │                   │
│                                  └─────────────────┘                   │
│                                                                         │
│  Tiết kiệm: (1510.9 - 1123.3) / 1510.9 = 25.6%                         │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 8.6 Bảng Phân tích Chi tiết

| Hoạt động | ODA-MD | OD | Lý do chênh lệch |
|-----------|--------|-----|------------------|
| **Request TX** | 17.6 µJ/batch | 17.6 µJ/batch | Giống nhau |
| **Data RX** | 38.4 µJ/batch | 38.4 µJ/batch | Giống nhau |
| **CPU Processing** | 10 µJ (matrix) | 2 µJ (cluster) | ODA-MD cao hơn |
| **Clustering Comm** | 0 | ~50 µJ/batch | OD cần trao đổi clustering |
| **Inter-CH Comm** | 0 | ~100 µJ/batch | OD cần giao tiếp giữa CHs |
| **Classification** | 0 | ~30 µJ/batch | OD phân loại Error/Event |
| **Forward TX** | ~50 µJ/clean | ~50 µJ/clean | Giống nhau |

### 8.7 Tổng hợp Năng lượng Sau 6000s Mô phỏng

| Thành phần | ODA-MD | OD |
|------------|--------|-----|
| Số requests gửi | 6000 | 6000 |
| Số samples nhận | ~18000 | ~18000 |
| Số outliers phát hiện | 970 | 451 |
| Số packets forward | ~17000 | ~15500 |
| **Tổng năng lượng CH** | **1123.3 mJ** | **1510.9 mJ** |
| **Chênh lệch** | - | **+34.5%** |

### 8.8 Code Triển khai Model Năng lượng

File `EnergyModel.h` triển khai đầy đủ mô hình Heinzelman:

```cpp
class EnergyModel {
  private:
    static constexpr double E_ELEC = 50e-9;   // 50 nJ/bit
    static constexpr double E_AMP = 100e-12;  // 100 pJ/bit/m²
    static constexpr double E_DA = 5e-9;      // 5 nJ/bit
    static constexpr double D0 = 87.0;        // Threshold distance (m)
    
  public:
    // Transmit k bits over distance d
    double transmit(int bits, double distance) {
        double energy;
        if (distance < D0) {
            // Free space model: E = E_elec*k + E_amp*k*d²
            energy = E_ELEC * bits + E_AMP * bits * distance * distance;
        } else {
            // Multipath model: E = E_elec*k + E_mp*k*d⁴
            double d4 = distance * distance * distance * distance;
            energy = E_ELEC * bits + 0.0013e-12 * bits * d4;
        }
        consumeEnergy(energy);
        return energy;
    }
    
    // Receive k bits
    double receive(int bits) {
        double energy = E_ELEC * bits;  // E = E_elec*k
        consumeEnergy(energy);
        return energy;
    }
    
    // CPU processing
    double process(int operations) {
        double energy = 10e-9 * operations;  // 10 nJ/op
        consumeEnergy(energy);
        return energy;
    }
};
```

### 8.9 Kết luận về Tiêu thụ Năng lượng

1. **ODA-MD tiết kiệm ~26% năng lượng** so với OD nhờ:
   - Không cần trao đổi thông tin clustering giữa sensors
   - Không cần giao tiếp inter-cluster giữa các CHs
   - Phân biệt Error/Event ngầm (không cần bước riêng)

2. **CPU Processing của ODA-MD cao hơn** (matrix inversion) nhưng:
   - Chỉ tiêu thụ ~10 µJ/batch
   - Nhỏ hơn nhiều so với chi phí communication của OD

3. **Communication là chi phí chính** trong WSN:
   - Transmit 256 bits qua 30m ≈ 35 µJ
   - So với receive (12.8 µJ) và process (10 µJ)

> **Kết luận**: Việc giảm số lượng tin nhắn trao đổi (communication overhead) là yếu tố quan trọng nhất để tiết kiệm năng lượng trong WSN. ODA-MD đạt được điều này bằng cách thực hiện tất cả tính toán locally tại Cluster Head.

---

## 9. Chi tiết Cơ chế Inject Outlier

### 9.1 Tổng quan

Để đánh giá hiệu quả của thuật toán ODA-MD, chúng ta cần inject outlier (dữ liệu bất thường) vào dataset Intel Lab. Theo bài báo gốc, **1000 outliers** được inject vào dataset với **multiplier = 5.0** (Đã thử với 1.5 và 2.5 ).

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    QUÁ TRÌNH INJECT OUTLIER                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Intel Lab Data (Real)                                                   │
│  ┌──────────────────────────────────────────────────────────────┐       │
│  │ T=22.5  H=38.2  L=480   V=2.45  │ Normal                     │       │
│  │ T=23.1  H=37.8  L=495   V=2.48  │ Normal                     │       │
│  │ T=22.8  H=38.5  L=488   V=2.46  │ Normal       ┌───────────┐ │       │
│  │ T=22.9  H=38.0  L=490   V=2.47  │ Normal  ───► │ INJECT!   │ │       │
│  │ T=23.0  H=37.9  L=492   V=2.46  │ Normal       └───────────┘ │       │
│  │ ...                              │                           │       │
│  └──────────────────────────────────────────────────────────────┘       │
│                                          │                               │
│                                          ▼                               │
│  Data với Outliers                                                       │
│  ┌──────────────────────────────────────────────────────────────┐       │
│  │ T=22.5  H=38.2  L=480   V=2.45  │ Normal                     │       │
│  │ T=23.1  H=37.8  L=495   V=2.48  │ Normal                     │       │
│  │ T=22.8  H=38.5  L=488   V=2.46  │ Normal                     │       │
│  │ T=114.5 H=7.6   L=4900  V=4.94  │ ★ OUTLIER (Type 0)        │       │
│  │ T=23.0  H=37.9  L=492   V=2.46  │ Normal                     │       │
│  │ ...                              │                           │       │
│  └──────────────────────────────────────────────────────────────┘       │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 9.2 Các Loại Outlier (Multivariate)

Để tạo sự đa dạng và đảm bảo outliers có thể xuất hiện theo nhiều dạng khác nhau, hàm `injectExactOutliers()` tạo **4 loại outlier** luân phiên:

| Type | Tên | Temperature | Humidity | Light | Voltage | 
|------|-----|-------------|----------|-------|---------|-------|
| **0** | High-T, Low-H, High-L | ×5 (+400%) | ÷5 (-80%) | ×10 (+900%) | ×2 (+100%) | 
| **1** | Low-T, High-H, Low-L | ÷5 (-80%) | ×5 (+400%) | ÷10 (-90%) | ÷2 (-50%) | 
| **2** | All High | ×5 | ×5 | ×5 | ×2.5 | 
| **3** | All Low | ÷5 | ÷5 | ÷5 | ÷2.5 | 

### 9.3 Ví dụ Cụ thể

**Giá trị gốc (Normal):**
```
T = 22.9°C    H = 38.0%    L = 490 lux    V = 2.47V
```

**Sau khi inject mỗi loại outlier (multiplier = 5.0 - đã thử nhiều multiplier khác kết quả không chênh nhiều):**

| Type | Temperature | Humidity | Light | Voltage |
|------|-------------|----------|-------|---------|
| **Normal** | 22.9°C | 38.0% | 490 lux | 2.47V |
| **Type 0** | **114.5°C** | **7.6%** | **4900 lux** | **4.94V** |
| **Type 1** | **4.58°C** | **190%** | **49 lux** | **1.24V** |
| **Type 2** | **114.5°C** | **190%** | **2450 lux** | **6.18V** |
| **Type 3** | **4.58°C** | **7.6%** | **98 lux** | **0.99V** |

### 9.4 Phân bố Outlier trong Dataset

```cpp
// Tính khoảng cách giữa các outlier để phân bố đều
int interval = allIndices.size() / targetCount;
// Với ~18000 readings và 1000 outliers: interval = 18
// → Cứ mỗi 18 readings thì có 1 outlier
```

**Thống kê phân bố:**

| Thông số | Giá trị |
|----------|---------|
| Tổng readings | ~18,000 |
| Số outliers inject | **1,000** |
| Tỷ lệ outlier | ~5.5% |
| Interval giữa outliers | ~18 readings |
| Phân bố theo sensor | Đều trên cả 3 sensors (36, 37, 38) |
| Random seed | 42 (cố định để reproducible) |



### 9.6 Lý do Sử dụng Multivariate Outliers

1. **Realistic**: Trong thực tế, lỗi sensor thường ảnh hưởng nhiều thuộc tính
2. **Challenging**: Test khả năng phát hiện outlier đa chiều của ODA-MD
3. **Diverse**: 4 loại outlier đảm bảo thuật toán không bị bias với 1 pattern


---

**Tài liệu tham khảo:**
1. Titouna et al., *"ODA-MD for WSN"*, ICCCI-2019
2. Fawzy et al., *"Outliers detection and classification in WSN"*, Egyptian Informatics Journal, 2013
3. Intel Lab Data: http://db.csail.mit.edu/labdata/labdata.html
4. W. B. Heinzelman, A. P. Chandrakasan, and H. Balakrishnan, *"An application-specific protocol architecture for wireless microsensor networks"*, IEEE Transactions on Wireless Communications, 2002
