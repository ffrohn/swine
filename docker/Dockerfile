FROM voidlinux/voidlinux-musl:latest as base

ENV CFLAGS -march=x86-64 -O2
ENV CXXFLAGS $CFLAGS
RUN echo "repository=https://repo-default.voidlinux.org/current/musl" > /etc/xbps.d/00-repository-main.conf
RUN xbps-install -yS xbps
RUN xbps-install -yS gcc git automake autoconf make cmake wget python-devel bash



FROM base as poly

RUN xbps-install -yS gmpxx-devel

RUN wget https://github.com/SRI-CSL/libpoly/archive/refs/tags/v0.1.13.tar.gz
RUN tar xf v0.1.13.tar.gz
WORKDIR /libpoly-0.1.13/build
RUN cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON ..
RUN make -j
RUN make install



FROM base as cvc5

RUN xbps-install -yS python3-devel python3-pip libtool texinfo cln-devel
ENV VIRTUAL_ENV=/opt/venv
RUN python3 -m venv $VIRTUAL_ENV
ENV PATH="$VIRTUAL_ENV/bin:$PATH"
RUN pip install tomli pyparsing

COPY --from=poly /usr/local/lib/libpoly.a /usr/local/lib/
COPY --from=poly /usr/local/lib/libpolyxx.a /usr/local/lib/
COPY --from=poly /usr/local/include/poly /usr/local/include/poly

RUN git clone https://github.com/ffrohn/cvc5
WORKDIR cvc5
RUN git checkout cvc5-1.0.8-musl
RUN ./configure.sh --static --no-statistics --auto-download --poly --cln --gpl --no-docs
WORKDIR /cvc5/build
RUN make -j4
RUN make install



FROM base as z3

RUN xbps-install -yS python3-devel

RUN wget https://github.com/Z3Prover/z3/archive/refs/tags/z3-4.12.2.tar.gz
RUN tar xf z3-4.12.2.tar.gz
WORKDIR /z3-z3-4.12.2
RUN mkdir build
WORKDIR /z3-z3-4.12.2/build
RUN cmake -DZ3_BUILD_LIBZ3_SHARED=FALSE -DCMAKE_BUILD_TYPE=Release ..
RUN make -j
RUN make install



# FROM base as z3-binary

# RUN xbps-install -yS python3-devel

# RUN wget https://github.com/Z3Prover/z3/archive/refs/tags/z3-4.12.2.tar.gz
# RUN tar xf z3-4.12.2.tar.gz
# WORKDIR /z3-z3-4.12.2
# RUN python scripts/mk_make.py --staticbin
# WORKDIR /z3-z3-4.12.2/build
# RUN make -j4
# RUN make install



FROM base as cudd

RUN git clone https://github.com/ivmai/cudd.git
WORKDIR /cudd
# make check fails when compiled with -DNDEBUG
RUN ./configure CFLAGS="$CFLAGS -fPIC" CXXFLAGS="$CXXFLAGS -fPIC"
RUN sed -i 's/aclocal-1.14/aclocal-1.16/g' Makefile
RUN sed -i 's/automake-1.14/automake-1.16/g' Makefile
RUN make -j
RUN make -j check
RUN make install



FROM base as gmp

RUN xbps-install -yS lzip

RUN wget https://gmplib.org/download/gmp/gmp-6.2.1.tar.lz
RUN lzip -d gmp-6.2.1.tar.lz
RUN tar xf gmp-6.2.1.tar
WORKDIR /gmp-6.2.1
RUN ./configure ABI=64 CFLAGS="$CFLAGS -fPIC" CPPFLAGS="$CXXFLAGS -DPIC" --host=x86_64-pc-linux-gnu --enable-cxx --prefix /gmp/
RUN make -j
RUN make -j check
RUN make install



FROM base as yices

RUN xbps-install -yS gperf gmp-devel

COPY --from=gmp /gmp /gmp

COPY --from=poly /usr/local/lib/libpoly.a /usr/local/lib/
COPY --from=poly /usr/local/include/poly /usr/local/include/poly

COPY --from=cudd /usr/local/lib/libcudd.a /usr/local/lib/libcudd.a
COPY --from=cudd /usr/local/include/cudd.h /usr/local/include/cudd.h

RUN git clone https://github.com/SRI-CSL/yices2.git
WORKDIR /yices2
RUN autoconf
RUN ./configure --enable-mcsat --with-pic-gmp=/gmp/lib/libgmp.a
RUN make -j
RUN make -j static-lib
RUN make install
RUN cp -r /yices2/build/x86_64-pc-linux-gnu-release/dist/lib /yices2/build/lib



FROM base as smt-switch

COPY --from=z3 /z3-z3-4.12.2 /z3
COPY --from=z3 /usr/local/include/z3*.h /usr/local/include/

COPY --from=poly /usr/local/include/poly /usr/local/include/poly
COPY --from=poly /usr/local/lib/libpicpoly.a /usr/local/lib/
COPY --from=poly /usr/local/lib/libpicpolyxx.a /usr/local/lib/

COPY --from=cvc5 /cvc5 /cvc5

COPY --from=yices /yices2 /yices2

RUN xbps-install -yS bison flex gmp-devel gmpxx-devel

RUN git clone https://github.com/ffrohn/smt-switch.git
WORKDIR smt-switch
RUN git checkout exp
RUN ./configure.sh --without-tests --static --smtlib-reader --z3-home=/z3 --cvc5-home=/cvc5 --yices2-home=/yices2
WORKDIR /smt-switch/build
RUN make -j
RUN make install



FROM voidlinux/voidlinux-musl:latest as swine-docker

RUN mkdir /my_include/
RUN mkdir /my_lib/

COPY --from=z3 /usr/local/lib64/libz3.a /my_lib
COPY --from=z3 /usr/local/include/z3*.h /my_include

COPY --from=poly /usr/local/include/poly /my_include/poly
COPY --from=poly /usr/local/lib/libpoly.a /my_lib
COPY --from=poly /usr/local/lib/libpolyxx.a /my_lib

COPY --from=cudd /usr/local/include/cudd.h /my_include
COPY --from=cudd /usr/local/lib/libcudd.a /my_lib

COPY --from=yices /usr/local/include/yices*.h /my_include
COPY --from=yices /usr/local/lib/libyices.a /my_lib

COPY --from=smt-switch /usr/local/lib/libsmt-switch* /my_lib
COPY --from=smt-switch /usr/local/include/smt-switch /my_include/smt-switch
