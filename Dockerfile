FROM alpine:latest

ARG BOOST_VERSION=1.74.0
ARG BOOST_DIR=boost_1_74_0
ENV BOOST_VERSION ${BOOST_VERSION}

ARG GSL_VERSION=2.4
ARG GSL_DIR=gsl-2.4
ENV GSL_VERSION ${GSL_VERSION}

RUN apk add openssl \
    linux-headers \
    build-base

USER root

WORKDIR /usr/src
COPY library/ .

RUN tar zxvf ${BOOST_DIR}.tar.gz \
    && cd ${BOOST_DIR} \
    && ./bootstrap.sh
    
RUN cd ${BOOST_DIR} && ./b2 --without-python --prefix=/usr/local -j 4 install || true \
    && cd .. && rm -rf ${BOOST_DIR} && rm ${BOOST_DIR}.tar.gz

RUN tar zxvf ${GSL_DIR}.tar.gz \
    && cd ${GSL_DIR} \
    && ./configure
    
RUN cd ${GSL_DIR} && make && make install \
    && cd .. && rm -rf ${GSL_DIR} && rm ${GSL_DIR}.tar.gz

RUN apk add cmake
ENV PYTHONUNBUFFERED=1
RUN apk add --update --no-cache python3 python3-dev cython freetype-dev && ln -sf python3 /usr/bin/python
RUN python3 -m ensurepip
RUN pip3 install --no-cache --upgrade pip setuptools
RUN apk add openmp --repository=http://dl-cdn.alpinelinux.org/alpine/edge/testing/

COPY src/ ./DRGRAPH/src
COPY build.sh ./DRGRAPH
COPY CMakeLists.txt ./DRGRAPH
# COPY data/ ./DRGRAPH/data

RUN cd DRGRAPH && mkdir build && sh build.sh
# RUN pip install wheel && cd DRGRAPH/system/NetVback && pip install -r requirements.txt

# EXPOSE 9098
# CMD ["sh", "-c", "cd /usr/src/DRGRAPH/system/NetVback && python router.py"]
