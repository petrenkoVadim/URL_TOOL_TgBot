# ===== Етап 1: Збірка =====
FROM fedora:43 AS builder

RUN dnf install -y \
    gcc-c++ \
    cmake \
    git \
    boost-devel \
    openssl-devel \
    libcurl-devel \
    tgbot-cpp-devel \
    && dnf clean all

WORKDIR /opt
RUN git clone --depth 1 https://github.com/yhirose/cpp-httplib.git

WORKDIR /app
COPY main.cpp .
RUN g++ -std=c++17 main.cpp -o tg_bot \
    -I/opt/cpp-httplib \
    -lTgBot -lboost_system -lssl -lcrypto -lpthread -lcurl

# ===== Етап 2: Фінальний образ =====
FROM fedora:43

# Підключаємо RPM Fusion для ffmpeg (Fedora не включає його за замовчуванням)
RUN dnf install -y \
    https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-43.noarch.rpm \
    https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-43.noarch.rpm \
    && dnf install -y \
        tgbot-cpp \
        boost \
        openssl-libs \
        libcurl \
        ffmpeg \
        python3 \
        python3-pip \
    && dnf clean all

RUN pip install --no-cache-dir -U yt-dlp

COPY --from=builder /app/tg_bot /app/tg_bot

WORKDIR /app
EXPOSE 8080

CMD ["./tg_bot"]