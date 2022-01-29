from archlinux

run pacman -Syu --noconfirm
run pacman -S --noconfirm gcc cmake make

copy . /app
run rm -fr /app/build
run rm -fr /app/.env

workdir /app

run pacman -S --noconfirm `cat ./pkgs`

run cmake -DCMAKE_BUILD_TYPE=Realse -S . -B build
run cmake --build build
