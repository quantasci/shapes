
cmake CMakeLists.txt \
    -DLIBMIN_ROOT=../libmin \
    -B../build/shapes \
    -DBUILD_CUDA=false \
    -DBUILD_OPENGL=true \
    -DBUILD_GLEW=true \
    -DBUILD_GLTF=true

make -C../build/shapes
