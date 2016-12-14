#!/usr/bin/env bash

function mason_build {
    ${MASON_DIR}/mason install llvm ${MASON_VERSION}
    CLANG_PREFIX=$(${MASON_DIR}/mason prefix llvm ${MASON_VERSION})

    MAJOR_MINOR=$(echo $MASON_VERSION | cut -d '.' -f1-2)

    # copy bin
    mkdir -p "${MASON_PREFIX}/bin"
    cp -a "${CLANG_PREFIX}/bin/${MASON_NAME}" "${MASON_PREFIX}/bin/"
    cp -a "${CLANG_PREFIX}/bin/${MASON_NAME}-${MAJOR_MINOR}" "${MASON_PREFIX}/bin/"
    cp -a "${CLANG_PREFIX}/bin/clang" "${MASON_PREFIX}/bin/"
    cp -a "${CLANG_PREFIX}/bin/clang-${MAJOR_MINOR}" "${MASON_PREFIX}/bin/"
    cp -a "${CLANG_PREFIX}/bin/llvm-symbolizer" "${MASON_PREFIX}/bin/"

    # copy share
    mkdir -p "${MASON_PREFIX}/share"
    cp -r "${CLANG_PREFIX}/share/clang" "${MASON_PREFIX}/share/"
    # copy include/c++
    mkdir -p "${MASON_PREFIX}/include"
    cp -r "${CLANG_PREFIX}/include/c++" "${MASON_PREFIX}/include/"
    # copy libs
    mkdir -p "${MASON_PREFIX}/lib"
    cp -r ${CLANG_PREFIX}/lib/libc++* "${MASON_PREFIX}/lib/"
    cp -r ${CLANG_PREFIX}/lib/libLTO.* "${MASON_PREFIX}/lib/"
    if [[ $(uname -s) == 'Linux' ]]; then
        cp -r ${CLANG_PREFIX}/lib/LLVMgold* "${MASON_PREFIX}/lib/"
    fi
    mkdir -p "${MASON_PREFIX}/lib/clang"
    cp -R ${CLANG_PREFIX}/lib/clang/${MASON_VERSION} "${MASON_PREFIX}/lib/clang/"

}

function mason_cflags {
    :
}

function mason_ldflags {
    :
}

function mason_static_libs {
    :
}
