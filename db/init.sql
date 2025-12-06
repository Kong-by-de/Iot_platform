INSERT INTO user_alerts (chat_id, temp_high_threshold, temp_low_threshold, hum_high_threshold, hum_low_threshold) 
VALUES (1067054337, 28.0, 15.0, 70.0, 30.0) 
ON CONFLICT (chat_id) DO NOTHING;

INSERT INTO user_devices (chat_id, device_id) 
VALUES 
    (1067054337, 'sensor_1'),
    (1067054337, 'sensor_2'),
    (1067054337, 'test_device')
ON CONFLICT (chat_id, device_id) DO NOTHING;

INSERT INTO telemetry_data (device_id, temperature, humidity) 
VALUES 
    ('sensor_1', 22.5, 55.0),
    ('sensor_2', 24.0, 60.0),
    ('test_device', 35.0, 75.0)
ON CONFLICT DO NOTHING;