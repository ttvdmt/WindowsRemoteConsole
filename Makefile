build:
	g++ src/main.cpp src/server/server.cpp src/client/client.cpp src/service/service.cpp src/utils.cpp -o console.exe -lws2_32 -ladvapi32 -static

client: build
	console.exe -c

server: build
	console.exe -s

service-install: build
	console.exe -install

service-uninstall: build
	console.exe -unistall

service-run: service-install
	console.exe -run
