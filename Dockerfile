FROM voidlinux/voidlinux-musl:latest as base

ENV CFLAGS -march=x86-64 -O2
ENV CXXFLAGS $CFLAGS
RUN echo "repository=https://repo-default.voidlinux.org/current/musl" > /etc/xbps.d/00-repository-main.conf
RUN xbps-install -yS xbps
RUN xbps-install -ySu
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



FROM base as smt-switch

COPY --from=z3 /z3-z3-4.12.2 /z3
COPY --from=z3 /usr/local/include/z3*.h /usr/local/include/

COPY --from=poly /usr/local/include/poly /usr/local/include/poly
COPY --from=poly /usr/local/lib/libpicpoly.a /usr/local/lib/
COPY --from=poly /usr/local/lib/libpicpolyxx.a /usr/local/lib/

COPY --from=CVC5 /cvc5 /cvc5

RUN xbps-install -yS bison flex gmp-devel gmpxx-devel

ARG CACHE_BUST=1

RUN git clone https://github.com/ffrohn/smt-switch.git
WORKDIR smt-switch
RUN git checkout exp
RUN ./configure.sh --without-tests --static --smtlib-reader --z3-home=/z3 --cvc5-home=/cvc5
WORKDIR /smt-switch/build
RUN make -j
RUN make install



FROM voidlinux/voidlinux-musl:latest as swine-docker

COPY --from=z3 /usr/local/lib64/libz3.a /usr/local/lib64/
COPY --from=z3 /usr/local/include/z3*.h /usr/local/include/

COPY --from=poly /usr/local/include/poly /usr/local/include/poly
COPY --from=poly /usr/local/lib/libpoly.a /usr/local/lib/
COPY --from=poly /usr/local/lib/libpolyxx.a /usr/local/lib/

COPY --from=CVC5 /usr/local/lib64/libcvc5.a /usr/local/lib/libcvc5.a
COPY --from=CVC5 /usr/local/include/cvc5 /usr/local/include/cvc5
COPY --from=CVC5 /usr/local/lib64/libcadical.a /usr/local/lib/libcadical.a

COPY --from=smt-switch /usr/local/lib/libsmt-switch* /usr/local/lib/
COPY --from=smt-switch /usr/local/include/smt-switch /usr/local/include/smt-switch
