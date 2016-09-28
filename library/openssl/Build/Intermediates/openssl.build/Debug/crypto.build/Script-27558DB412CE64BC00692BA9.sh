#!/bin/sh
if [ -f "$CONFIGURATION_BUILD_DIR/libcrypto.a" ]; then
	exit 0;
fi

OPENSSL_SRCROOT="$SRCROOT/openssl"
if [ ! -d "$OPENSSL_SRCROOT" ]; then
    OPENSSL_SRCROOT="$SRCROOT"
fi;

SRC_ARCHIVE=`ls openssl*tar.gz 2>/dev/null`
if [ -f "$SRC_ARCHIVE" ]; then
	OPENSSL_SRCROOT="$PROJECT_TEMP_DIR/openssl"
	if [ ! -d "$OPENSSL_SRCROOT" ]; then
		echo "Extracting $SRC_ARCHIVE..."
		mkdir "$OPENSSL_SRCROOT"
		tar -C "$OPENSSL_SRCROOT" --strip-components=1 -zxf "$SRC_ARCHIVE" || exit 1
		cp -RL "$OPENSSL_SRCROOT/include" "$CONFIGURATION_BUILD_DIR"
	fi
fi

if [ "$SDKROOT" != "" ]; then
	ISYSROOT="-isysroot $SDKROOT"
fi

OPENSSL_OPTIONS="no-krb5 no-gost"

cd "$OPENSSL_SRCROOT"

## this is a universal build
if [ "$ARCHS_STANDARD_32_BIT" = "i386 ppc" ]; then

	BUILDARCH="ppc"
	echo "***** BUILDING UNIVERSAL ARCH $BUILDARCH ******"
	make clean
	./config no-asm $OPENSSL_OPTIONS -openssldir="$BUILD_DIR"
	ASM_DEF="-UOPENSSL_BN_ASM_PART_WORDS"
	make CC=$PLATFORM_DEVELOPER_BIN_DIR/gcc CFLAG="-D_DARWIN_C_SOURCE $ASM_DEF -arch $BUILDARCH $ISYSROOT" SHARED_LDFLAGS="-arch $BUILDARCH -dynamiclib"
	cp libcrypto.a "$CONFIGURATION_TEMP_DIR"/$BUILDARCH-libcrypto.a
	cp libssl.a "$CONFIGURATION_TEMP_DIR"/$BUILDARCH-libssl.a
	
	BUILDARCH="i386"
	echo "***** BUILDING UNIVERSAL ARCH $BUILDARCH ******"
	make clean
	./config $OPENSSL_OPTIONS -openssldir="$BUILD_DIR"
	ASM_DEF="-DOPENSSL_BN_ASM_PART_WORDS"
	make CC=$PLATFORM_DEVELOPER_BIN_DIR/gcc CFLAG="-D_DARWIN_C_SOURCE $ASM_DEF -arch $BUILDARCH $ISYSROOT" SHARED_LDFLAGS="-arch $BUILDARCH -dynamiclib"
	cp libcrypto.a "$CONFIGURATION_TEMP_DIR"/$BUILDARCH-libcrypto.a
	cp libssl.a "$CONFIGURATION_TEMP_DIR"/$BUILDARCH-libssl.a
	mkdir -p "$CONFIGURATION_BUILD_DIR"
	lipo -create "$CONFIGURATION_TEMP_DIR"/*-libcrypto.a -output "$CONFIGURATION_BUILD_DIR/libcrypto.a"
   	lipo -create "$CONFIGURATION_TEMP_DIR"/*-libssl.a -output "$CONFIGURATION_BUILD_DIR/libssl.a"
else
	BUILDARCH=$ARCHS
	echo "***** BUILDING ARCH $BUILDARCH ******"
	make clean

	if [ "$BUILDARCH" = "i386" ]; then
		./config $OPENSSL_OPTIONS -openssldir="$BUILD_DIR"
		ASM_DEF="-DOPENSSL_BN_ASM_PART_WORDS"
     else
		./config no-asm $OPENSSL_OPTIONS -openssldir="$BUILD_DIR"
		ASM_DEF="-UOPENSSL_BN_ASM_PART_WORDS"
     fi
	make CC=$PLATFORM_DEVELOPER_BIN_DIR/gcc CFLAG="-D_DARWIN_C_SOURCE $ASM_DEF -arch $BUILDARCH $ISYSROOT" SHARED_LDFLAGS="-arch $BUILDARCH -dynamiclib"
	mkdir -p "$CONFIGURATION_BUILD_DIR"
	cp libcrypto.a "$CONFIGURATION_BUILD_DIR"
	cp libssl.a "$CONFIGURATION_BUILD_DIR"
fi

ranlib "$CONFIGURATION_BUILD_DIR/libcrypto.a"
ranlib "$CONFIGURATION_BUILD_DIR/libssl.a"
