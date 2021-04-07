#!/bin/bash

echo '1. starting mysql...'

service mysql start

echo '2. starting ssh...'
service ssh start

tail -f /dev/null