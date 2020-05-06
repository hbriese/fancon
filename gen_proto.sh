#!/bin/sh

SRC=proto
FILE=$SRC/DevicesSpec.proto
CPP=src/proto
JS=../fanconw/src/proto

DIR=$(dirname "$0")
cd "$DIR"

# fancon
mkdir -p ./$CPP
rm -f ./$CPP/*

protoc -I=./$SRC ./$FILE \
    --cpp_out=./$CPP \
    --grpc_out=./$CPP \
    --plugin=protoc-gen-grpc=`which grpc_cpp_plugin`

# Silence compiler warnings from protoc generated code
CLANG_DIAG_S=$(cat <<-END
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#elif __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif //__clang__
END
)
CLANG_DIAG_E=$(cat <<-END
#ifdef __clang__
#pragma clang diagnostic pop
#elif __GNUC__
#pragma GCC diagnostic pop
#endif //__clang__
END
)
for filename in ./$CPP/*.h; do
	echo "$CLANG_DIAG_S\n\n$(cat ./$filename)\n\n$CLANG_DIAG_E" > ./$filename
done

# fanconw
#npm install -g grpc-tools protoc-gen-grpc-web
#mkdir -p ./$JS
#rm -f ./$JS/*
#
#protoc -I=./$SRC ./$FILE \
#    --js_out=import_style=commonjs:$JS \
#    --grpc-web_out=import_style=commonjs,mode=grpcwebtext:$JS \
#    --plugin=protoc-gen-grpc-web=`which protoc-gen-grpc-web`
#
## Prepend /* eslint-disable */ to ignore compiler complaints
#for filename in ./$JS/*.js; do
#	echo "/* eslint-disable */\n$(cat ./$filename)" > ./$filename
#done

