#!/bin/bash
cd "$(dirname "$0")"
DATABASE_URL="postgres://iot_user:pass2025@localhost:5432/iot_devices" dbmate up