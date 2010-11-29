#!/bin/sh

VERSION="alpha"

BIN="bin"
PORT=12345

YACC_FLAGS="-DYYERROR_VERBOSE" # for bison
WARN="-W -Wall -Wextra -Wredundant-decls -Wno-unused"
CC="gcc -g -std=c99 $WARN $YACC_FLAGS"
YACC="yacc -d"
LEX="flex -I"

[ "Linux" = `uname` ] && CC="$CC -pthread"
[ "SunOS" = `uname` ] && CC="$CC -lsocket"

LIBS="array% expression% head% http% memory% mutex% pack% relation%"
LIBS="$LIBS string% summary% system% tuple% transaction% value% version%"
LIBS="$LIBS volume% test/common% lex.yy% y.tab%"
STRUCT_TESTS="test/array% test/expression% test/head% test/http% test/language%"
STRUCT_TESTS="$STRUCT_TESTS test/memory% test/multiproc% test/number%"
STRUCT_TESTS="$STRUCT_TESTS test/pack% test/relation% test/string%"
STRUCT_TESTS="$STRUCT_TESTS test/summary% test/system% test/tuple%"
STRUCT_TESTS="$STRUCT_TESTS test/transaction% test/value%"
PERF_TESTS="test/perf/expression% test/perf/multiproc% test/perf/number%"
PERF_TESTS="$PERF_TESTS test/perf/relation% test/perf/system% test/perf/tuple%"
PROGS="bandicoot%"

ALL_VARS=`ls test/data`

create_out_dirs()
{
    mkdir -p "$BIN/test/perf"
    mkdir -p "$BIN/volume"
    mkdir -p "$BIN/test/lsdir/one_dir"
}

check_lp64()
{
    lp64=`mktemp lp64-XXXX`
    echo 'int main() { return sizeof(long); }' \
        | $CC -o $lp64 -x c -

    ./$lp64
    if [ "8" = "$?" ]
    then
        echo '[=] LP64'
        CC="$CC -DLP64"
    else
        echo '[=] ILP32'
    fi

    rm -f $lp64
}

prepare_lang()
{
    $YACC language.y
    $LEX language.l
}

prepare_version()
{
    cat > version.c << EOF
const char *VERSION =
    "[bandicoot $VERSION, http://bandilab.org, built on `date -u`]\n";
EOF
}

compile()
{
    create_out_dirs
    prepare_lang
    prepare_version
    check_lp64
    echo

    for i in `echo $* | sed 's/%//g'`
    do
        echo "[C] $i.o"
        $CC $DIST_CFLAGS -c -o $i.o $i.c
    done
}

link()
{
    libs=`echo $LIBS | sed 's/%/.o/g'`
    for obj in `echo $* | sed 's/%/.o/g'`
    do
        e=`echo $BIN/$obj | sed 's/\.o//g'`
        echo "[L] $e"
        $CC $DIST_CFLAGS -o $e $libs $obj
    done
}

run()
{
    for e in `echo $* | sed 's/%//g'`
    do
        echo "[T] $e"
        $BIN/$e
    done
}

clean()
{
    find . -name '*.o' -exec rm {} \;
    find . -name '*.log' -exec rm {} \;
    rm -rf version.c y.tab.[ch] lex.yy.c $BIN
}

dist()
{
    DIST_CFLAGS=$*

    clean
    compile $LIBS $PROGS
    echo
    link $PROGS
    echo

    d="bandicoot-$VERSION"
    mkdir -p $BIN/$d
    cp LICENSE NOTICE $BIN/bandicoot $BIN/$d && cd $BIN

    a="`pwd`/$d.tar.gz"
    echo "[A] $a"
    tar cfz $a $d
}

cmd=$1
case $cmd in
    clean)
        clean
        ;;
    pack)
        clean
        compile $LIBS $PROGS $STRUCT_TESTS $PERF_TESTS
        echo
        link $PROGS $STRUCT_TESTS $PERF_TESTS
        echo
        echo "[P] ..."
        $BIN/bandicoot deploy -v bin/volume -s test/test_defs.b
        $BIN/bandicoot start -v bin/volume -p $PORT &
        pid=$!

        # we need to finish the initialization
        # before we start storing the variables
        sleep 1

        for v in $ALL_VARS
        do
            curl -s --data-binary @test/data/$v http://localhost:$PORT/store_$v
        done
        kill $pid
        ;;
    perf)
        run $PERF_TESTS 
        ;;
    stress_read)
        while [ true ]
        do
            for v in $ALL_VARS
            do
                curl -s http://localhost:$PORT/load_$v -o /dev/null
                if [ "$?" != "0" ]
                then
                    exit
                fi
            done
        done
        ;;
    stress_write)
        while [ true ]
        do
            for v in $ALL_VARS
            do
                curl -s --data-binary @test/data/$v \
                    http://localhost:$PORT/store_$v -o /dev/null
                if [ "$?" != "0" ]
                then
                    exit
                fi
            done
        done
        ;;
    test)
        run $STRUCT_TESTS
        cat test/progs/*.log | sort > bin/lang_test.out
        diff -u bin/lang_test.out test/progs/all_sorted.out
        if [ "$?" != "0" ]
        then
            echo "---"
            echo "language test failed (inspect the differences above)"
        fi
        ;;
    todos)
        find . -regex '.*\.[chly]' -exec egrep -H -i 'fixme|todo|think' {} \;
        ;;
    dist)
        dist "-Os $2"
        ;;
    *)
        echo "unknown command '$cmd', usage: ctl <command>"
        echo "    dist - build a package for distribution"
        echo "    pack - compile and prepare for running tests"
        echo "    test - execute structured tests (prereq: pack)"
        echo "    perf - execute performance tests (prereq: pack)"
        echo "    clean - remove object files etc."
esac
