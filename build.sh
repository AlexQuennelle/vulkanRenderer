if [[ "$1" ]]; then
	buildType="$1"
else
	read -p "Build type: " buildType
fi
mkdir -p bin
if [ "${buildType^}" != "Debug" ] && [ "${buildType^}" != "Release" ]; then
	buildType="Debug"
fi
mkdir -p build
cmake -S . -B ./build -G "Ninja" -DCMAKE_BUILD_TYPE="${buildType^}"
cmake --build ./build
exit
