-- migrate:up
CREATE TABLE telemetry_data (
    id SERIAL PRIMARY KEY,
    device_id TEXT NOT NULL,
    temperature REAL NOT NULL,
    humidity REAL NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT valid_temperature CHECK (temperature >= -50 AND temperature <= 100),
    CONSTRAINT valid_humidity CHECK (humidity >= 0 AND humidity <= 100)
);

CREATE INDEX idx_telemetry_device_id ON telemetry_data(device_id);
CREATE INDEX idx_telemetry_timestamp ON telemetry_data(timestamp DESC);

CREATE TABLE user_devices (
    id SERIAL PRIMARY KEY,
    chat_id BIGINT NOT NULL,
    device_id TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(chat_id, device_id)
);

CREATE TABLE user_alerts (
    chat_id BIGINT PRIMARY KEY,
    temp_high_threshold REAL DEFAULT 0,
    temp_low_threshold REAL DEFAULT 0,
    hum_high_threshold REAL DEFAULT 0,
    hum_low_threshold REAL DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT valid_temp_high CHECK (temp_high_threshold >= 0 AND temp_high_threshold <= 100),
    CONSTRAINT valid_temp_low CHECK (temp_low_threshold >= 0 AND temp_low_threshold <= 100),
    CONSTRAINT valid_hum_high CHECK (hum_high_threshold >= 0 AND hum_high_threshold <= 100),
    CONSTRAINT valid_hum_low CHECK (hum_low_threshold >= 0 AND hum_low_threshold <= 100)
);

-- migrate:down
DROP TABLE IF EXISTS user_alerts;
DROP TABLE IF EXISTS user_devices;
DROP TABLE IF EXISTS telemetry_data;