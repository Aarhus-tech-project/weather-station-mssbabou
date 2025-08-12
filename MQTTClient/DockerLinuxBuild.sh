# from project root (where CMakeLists.txt + .git live)
rm -rf build-amd64

# Build AMD64 tool image
docker buildx build --platform linux/amd64 -t mqttclient-build-amd64 -f Dockerfile .

# Build inside AMD64 container
docker run --rm --platform linux/amd64 -v "$PWD":/work -w /work mqttclient-build-amd64 \
  bash -lc '
    if [ -d .git ]; then
      git submodule update --init --recursive
    else
      echo "No .git found — skipping submodules. Ensure external/paho.mqtt.c and external/jsmn/jsmn.h exist."
    fi

    cmake -S . -B build-amd64 -DCMAKE_BUILD_TYPE=Release
    cmake --build build-amd64 -j
    file build-amd64/MQTTClient
    ldd build-amd64/MQTTClient || true
  '