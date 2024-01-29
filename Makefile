SHELL := /bin/bash

.PHONY: all
all: rebuild start

.PHONY: build
build:
	docker-compose build

.PHONY: rebuild
rebuild:
	docker-compose build --force-rm

.PHONY: start
start:
	docker-compose up -d

.PHONY: stop
stop:
	docker-compose stop

.PHONY: restart
restart: stop start

.PHONY: logs
logs:
	docker-compose logs -f --tail 100

.PHONY: clear
clear:
	(docker-compose kill  && \
	docker-compose rm -f) || \
	echo no containers to remove

.PHONY: local
local: conan-rebuild local-rebuild

.PHONY: local-rebuild
local-rebuild:
	cd comments-service/build && \
	source conanbuild.sh && \
	cmake -DCMAKE_BUILD_TYPE=Release .. && \
	cmake --build . && \
	source deactivate_conanbuild.sh && \
	./comments-service 0.0.0.0 8080

.PHONY: conan-rebuild
conan-rebuild:
	rm -rf comments-service/build/ && \
	mkdir comments-service/build && \
	cd comments-service/build && \
	conan install .. --output-folder=. --build=missing

.PHONY: bd-cqlsh
bd-cqlsh:
	docker exec -it scylla-node1 cqlsh