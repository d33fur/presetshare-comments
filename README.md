# presetshare-comments-service
## About
The service is written in **c++** and uses **boost.beast**, **cassandra-cpp-driver**, **ScyllaDB**.

**Conan** and **cmake** are used to download packages and build the project.

### ScyllaDB

|**KEYSPACE**|keyspace_comments|
|----|----|
|**TABLE**|comments|

|**column name**|entity|comment id|author|text|deleted|created_by|created_time|updated_time|
|----|----|----|----|----|----|----|----|----|
|**data type**|text|uuid|text|text|boolean|bigint|bigint|bigint|

## Usage
First of all you need to install make, docker and docker-compose:
```bash
sudo apdate && sudo apt install -y install make docker-ce docker-ce-cli containerd.io docker-compose-plugin
```
Clone the repository:
```bash
git clone https://github.com/d33fur/presetshare-comments-service.git && cd presetshare-comments-service
```
start all
```bash
make all
```
logs
```bash
make logs
```
start
```bash
make start
```
stop
```bash
make stop
```
restart
```bash
make restart
```
clear
```bash
make clear
```
to enter sqlsh
```bash
make bd-cqlsh
```

--------
Also i made a Dockerfile if you want to use **scylla-cpp-driver**(I do not know if the current code is compatible):
```dockerfile
FROM ubuntu:18.04 AS build

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Moscow

COPY ./comments-service /comments-service
WORKDIR /comments-service/build

SHELL ["/bin/bash", "-c"]

RUN apt-get update && \
    apt-get install -y \
    software-properties-common \
    python3-pip \
    libuv1 \
    openssl \
    libssl-dev \
    zlib1g \
    wget && \
    wget https://github.com/scylladb/cpp-driver/releases/download/2.15.2-1/scylla-cpp-driver_2.15.2-1_amd64.deb \
    https://github.com/scylladb/cpp-driver/releases/download/2.15.2-1/scylla-cpp-driver-dev_2.15.2-1_amd64.deb && \
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null && \
    apt-add-repository 'deb https://apt.kitware.com/ubuntu/ bionic main' && \
    apt-get update && \
    apt-get install -y \
    cmake \
    ./scylla-cpp-driver_2.15.2-1_amd64.deb \
    ./scylla-cpp-driver-dev_2.15.2-1_amd64.deb && \
    pip3 install conan --upgrade && \
    conan profile detect && \
    conan install .. --output-folder=. --build=missing && \ 
    source conanbuild.sh && \
    cmake -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . && \
    source deactivate_conanbuild.sh

FROM ubuntu:18.04

RUN groupadd dev && useradd -g dev dev
USER dev
COPY --chown=dev:dev --from=build /comments-service/build/comments-service /app/comments-service

CMD ["/app/comments-service", "0.0.0.0", "8080"]
EXPOSE 8080
```