// src/messages/SensorMsg.msg

// Request message from CH to Sensors (Algorithm 1 - ODA-MD paper)
message RequestMsg {
    int requestId;          // Request sequence number
}

// Response message from Sensors to CH
message SensorMsg {
    int sourceId;           // ID của nút gửi (để biết ai gửi)
    double temperature;     // Dữ liệu nhiệt độ
    double light;           // Dữ liệu ánh sáng
    double voltage;         // Dữ liệu điện áp
    double humidity;        // Dữ liệu độ ẩm
    
    bool isOutlier;         // (Optional) Đánh dấu xem đây có phải là nhiễu giả lập không
}