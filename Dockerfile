FROM ubuntu:latest

RUN apt-get update && \
    apt-get install -y \
    bash \
    cmake \
    python3 \
    pipx \
    git \
    build-essential \
    software-properties-common \
    gnupg \
    apt-utils \
    pkg-config \
    lsb-release \
    wget && \
    rm -rf /var/lib/apt/lists/*

ENV PATH='/root/.local/bin':$PATH

RUN pipx ensurepath && \
    pipx install conan && \
    pipx install ninja

RUN conan profile detect --force
RUN sed --in-place 's/gnu17/23/g' /root/.conan2/profiles/default

RUN git clone https://github.com/4J-company/conan-center-index.git
RUN conan remote add 4J-company ./conan-center-index --type local-recipes-index

RUN wget https://apt.llvm.org/llvm.sh && \
    chmod u+x llvm.sh && \
    ./llvm.sh 20 && \
    mkdir -p /etc/apt/keyrings && \
    mv /etc/apt/trusted.gpg.d/apt.llvm.org.asc /etc/apt/keyrings/
RUN apt-get install -y clang-20 lld-20 clang-tools-20 llvm-20-dev llvm-20-tools libc++-20-dev

ENV CC=clang-20
ENV CXX=clang-20

WORKDIR /app

COPY . .

RUN conan build . \
    -b missing \
    -c tools.system.package_manager:mode=install \
    -s folly/2024.08.12.00:compiler.cppstd=20

CMD ["/app/build/Release/model-renderer"]
