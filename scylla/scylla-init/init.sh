#!/bin/bash

# Ждем пока ScyllaDB запустится
while ! nc -z localhost 9042; do   
  sleep 1 # ждем 1 секунду перед повторной проверкой
done

# Выполняем скрипт инициализации
cqlsh -f /docker-entrypoint-initdb.d/init.cql
