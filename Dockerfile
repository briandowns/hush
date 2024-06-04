FROM ubuntu:latest

RUN apt-get update &&  \
    apt-get install -y \
    git                \
    make               \
    clang-6.0          \
    libulfius-dev      \
    libjansson-dev     \
    libmicrohttpd-dev

RUN make CC=clang-6.0

COPY . /

EXPOSE 8080:8080

CMD [ "/bin/emissary" ]