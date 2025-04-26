#!/bin/bash

# Create third_party directory if it doesn't exist
mkdir -p libs

# Download stb_image.h
curl -o libs/stb_image.h https://raw.githubusercontent.com/nothings/stb/master/stb_image.h

# Download stb_image_write.h
curl -o libs/stb_image_write.h https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h
