# presetshare-comments-service
## About
The service is written in **c++** and uses **boost/1.84.0**, **cassandra-cpp-driver/2.17.1**, **nlohmann_json/3.11.3**, **ScyllaDB/5.2**.

**Conan** and **cmake** are used to download packages and build the project.

### ScyllaDB

|**KEYSPACE**|**TABLE**|
|----|----|
|keyspace_comments|comments|

|**column name**|-|entity|comment id|author|text|deleted|created_by|created_time|updated_time|
|----|----|----|----|----|----|----|----|----|----|
|**data type**|-|text|uuid|text|text|boolean|bigint|bigint|bigint|

Additional details:

- ```**REPLICATION = {'class' : 'SimpleStrategy', 'replication_factor' : 3}**```

- ```**CLUSTERING ORDER BY (created_time DESC)**```

- ```**PRIMARY KEY ((entity), created_time, comment_id)**```

- Flags in docker-compose:
    - ```**--seeds=scylla-node1**``` seed nodes are the initial contact points for a ScyllaDB cluster
    - ```**--smp 1**``` number of cpu cores available for ScyllaDB
    - ```**--memory 750M**``` maximum amount of memory ScyllaDB should use
    - ```**--ovrprovisioned 1**``` indicates that ScyllaDB is running on an overprovisioned machine
    - ```**--api-adress 0.0.0.0**``` the Scylla API will listen on all network interfaces available to the container
## Usage
First of all you need to install make, docker and docker-compose:
```bash
sudo apdate && sudo apt install -y install make docker-ce docker-ce-cli containerd.io docker-compose-plugin
```
Clone the repository:
```bash
git clone https://github.com/d33fur/presetshare-comments-service.git && cd presetshare-comments-service
```
Start all
```bash
make all
```
Wait a bit and
```bash
make db-init
```
Logs
```bash
make logs
```
More commands:

Start
```bash
make start
```
Stop
```bash
make stop
```
Restart
```bash
make restart
```
Clear
```bash
make clear
```
To enter db sqlsh
```bash
make db-cqlsh
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