# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# docker buildx build . --target release --output type=local,dest=..

FROM alpine AS python-repo

RUN apk add git
RUN git clone https://github.com/python/cpython.git -b v3.13.2 --depth=1

FROM python-repo AS build-static

RUN apk add build-base linux-headers zlib-dev zlib-static
RUN sed -i '/^\*shared\*/,$ d' cpython/Modules/Setup.stdlib.in
RUN sed -i '/_testinternalcapi/ d' cpython/Modules/Setup.stdlib.in
COPY python.c cpython/Programs/python.c

RUN cd cpython && ./configure LDFLAGS="-static-pie" --disable-shared --without-mimalloc MODULE_BUILDTYPE=static
RUN make -C cpython LDFLAGS="-static-pie" LINKFORSHARED=" " -j$(nproc)
RUN strip -s cpython/python

FROM ubuntu AS build-so

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y git && \
    apt-get install -y clang libbfd-dev uthash-dev libelf-dev libcapstone-dev  libreadline-dev libiberty-dev libgsl-dev build-essential git debootstrap file

RUN git clone https://github.com/endrazine/wcc.git -b v0.0.7 --depth=1 --recursive
RUN make -C wcc -j$(nproc)

COPY --from=build-static cpython/python python
RUN wcc/src/wld/wld -libify python

FROM python-repo AS build-lib

RUN apk add python3
COPY py-trimmer.py /
COPY main.py cpython/Lib/main.py
RUN rm -r cpython/Lib/test cpython/Lib/unittest cpython/Lib/ensurepip
RUN find cpython/Lib/ -name 'test' -prune -exec rm -r {} \;
RUN find cpython/Lib/ -name 'tests' -prune -exec rm -r {} \;
RUN touch cpython/Lib/__main__.py
RUN python /py-trimmer.py cpython/Lib/
RUN python -m zipapp cpython/Lib/ -c -o Lib.zip

FROM alpine AS build-final
COPY --from=build-so python python
COPY --from=build-lib Lib.zip Lib.zip

RUN cat python Lib.zip > python.so

FROM scratch AS release
COPY --from=build-final python.so /
