#!/bin/bash
sudo docker build --platform linux/arm64 -t intercom_buildev_deb12 -f Dockerfile.debian12 .