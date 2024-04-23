FROM reg.supremind.info/hub/visionmind/cppbuild:v1 AS builder-amd64

ENV PATH /usr/local/cmake-3.17.2-Linux-x86_64/bin:$PATH
COPY . /opt/stream/
WORKDIR /opt/stream
RUN /bin/bash ./script/download_3rdparty.sh

WORKDIR /opt/stream/
RUN ./script/generated_grpc_code.sh

WORKDIR /opt/stream/build/
RUN cmake ../ -DCMAKE_BUILD_TYPE=Release && make -j4

FROM reg.supremind.info/hub/visionmind/aarch64-linux-gnu-gcc-7.5.0:v1 AS builder-arm64

ENV PATH /usr/local/cmake-3.17.2-Linux-x86_64/bin:$PATH
COPY . /opt/stream/
WORKDIR /opt/stream
RUN /bin/bash ./script/download_3rdparty.sh arm

WORKDIR /opt/stream/
RUN /bin/bash ./script/generated_grpc_code.sh arm

WORKDIR /opt/stream/build/
RUN cmake -DCMAKE_TOOLCHAIN_FILE=../aarch64-linux-gnu-gcc-7.5.0-toolchain.cmake ../ -DCMAKE_BUILD_TYPE=Release && make -j4

FROM reg.supremind.info/hub/visionmind/cpprunner:v1 as runner-amd64
RUN ln -sf /usr/share/zoneinfo/Asia/Shanghai /etc/localtime && echo "Asia/Shanghai" > /etc/timezone

RUN mkdir -p /workspace/lib/HCNetSDK/
RUN mkdir -p /etc/supremind/
WORKDIR /workspace
COPY --from=builder-amd64 /opt/stream/conf/stat.xsl /workspace/
COPY --from=builder-amd64 /opt/stream/3rdparty/HCNetSDK/lib/ /workspace/lib/HCNetSDK/
ENV LD_LIBRARY_PATH /workspace/lib/HCNetSDK/:$LD_LIBRARY_PATH

COPY --from=builder-amd64 /opt/stream/build/stream /workspace/stream

WORKDIR /workspace/
CMD [ "./stream", "/workspace/" ]

FROM reg.supremind.info/hub/ci/ubuntu:18.04-arm64 as runner-arm64
RUN ln -sf /usr/share/zoneinfo/Asia/Shanghai /etc/localtime && echo "Asia/Shanghai" > /etc/timezone

WORKDIR /workspace
COPY --from=builder-arm64 /opt/stream/conf/stat.xsl /workspace/
COPY --from=builder-arm64 /opt/stream/build/stream /workspace/stream

WORKDIR /workspace/
CMD [ "./stream", "/workspace/" ]

FROM runner-$TARGETARCH
