FROM ubuntu:16.04

RUN echo "deb http://ppa.launchpad.net/jonathonf/python-3.6/ubuntu xenial main" > /etc/apt/sources.list.d/jonathonf-ubuntu-python-3_6-xenial.list

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --allow-unauthenticated \
	build-essential \
	dh-make \
	g++ \
	make \
	dpkg-dev \
	libssl-dev \
	libpq-dev \
	openssl \
	python3.6-dev

COPY . /src/kore
WORKDIR /src/kore

ENV PYTHON=1
ENV PGSQL=1
ENV TASKS=1
RUN make clean
RUN make
RUN make install