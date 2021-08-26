FROM ubuntu:20.04

RUN apt update && apt install -y build-essential ca-certificates libssl-dev nlohmann-json3-dev

WORKDIR /app

COPY main.cc blobs.txt ./
COPY cpp-httplib/httplib.h cpp-httplib/httplib.h 

RUN g++ -Wall -Werror -std=c++17 -O3 -o main main.cc -lssl -lcrypto -lpthread

CMD ["/app/main"]