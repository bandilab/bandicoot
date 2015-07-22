#!/bin/sh

VERSION="v6"

BIN="bin"
PORT=12345

YACC_FLAGS="-DYYERROR_VERBOSE" # for bison
WARN="-W -Wall -Wextra -Wredundant-decls -Wno-unused"
LINK=""
CC="gcc -g -std=c99 $WARN $YACC_FLAGS"
YACC="yacc -d"
LEX="flex -I"

LIBS="array% convert% convert.lex% error% expression% head% http% index%"
LIBS="$LIBS memory% pack% relation% string% summary% tuple% transaction%"
LIBS="$LIBS value% variable% version% volume% test/common% lex.yy% y.tab%"
STRUCT_TESTS="test/array% test/expression% test/head% test/http% test/index%"
STRUCT_TESTS="$STRUCT_TESTS test/language% test/list% test/memory%"
STRUCT_TESTS="$STRUCT_TESTS test/multiproc% test/network% test/number%"
STRUCT_TESTS="$STRUCT_TESTS test/pack% test/relation% test/string%"
STRUCT_TESTS="$STRUCT_TESTS test/summary% test/system% test/tuple%"
STRUCT_TESTS="$STRUCT_TESTS test/transaction% test/value% test/bandicoot%"
PERF_TESTS="test/perf/expression% test/perf/index% test/perf/multiproc%"
PERF_TESTS="$PERF_TESTS test/perf/number% test/perf/relation%"
PERF_TESTS="$PERF_TESTS test/perf/system% test/perf/tuple%"
PROGS="bandicoot%"

if [ ! "`uname | grep -i CYGWIN`" = "" ]
then
    LIBS="$LIBS system_win32%"
    LINK="-lws2_32"
else
    LIBS="$LIBS system_posix%"
    if [ `uname` = "SunOS" ]
    then
        LINK="-lnsl -lsocket"
    elif [ `uname` = "Linux" ]
    then
        CC="$CC -pthread"
    fi
fi

ALL_VARS=`ls test/data`

create_out_dirs()
{
    mkdir -p "$BIN/test/perf"
    mkdir -p "$BIN/volume"
    mkdir -p "$BIN/test/lsdir/one_dir"
}

prepare_lang()
{
    $YACC language.y
    $LEX language.l
    $YACC -o convert.c -p conv_ convert.y
    $LEX -o convert.lex.c -P conv_ convert.l
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

    for i in `echo $* | sed 's/%//g'`
    do
        echo "[C] $i.o"
        $CC $DIST_CFLAGS -c -o $i.o $i.c
        if [ "$?" != "0" ]
        then
            echo "Compilation error. Abort."
            exit 1
        fi
    done
}

link()
{
    libs=`echo $LIBS | sed 's/%/.o/g'`
    for obj in `echo $* | sed 's/%/.o/g'`
    do
        e=`echo $BIN/$obj | sed 's/\.o//g'`
        echo "[L] $e"
        $CC $DIST_CFLAGS -o $e $libs $obj $LINK
        if [ "$?" != "0" ]
        then
            echo "Linking error. Abort."
            exit 1
        fi
    done
}

run()
{
    for e in `echo $* | sed 's/%//g'`
    do
        echo "[T] $e"
        $BIN/$e
        if [ "$?" != "0" ]
        then
            echo "    failed"
        fi
    done
}

clean()
{
    find . -name '*.o' -exec rm {} \;
    find . -name '*.log' -exec rm {} \;
    rm -rf version.c y.tab.[ch] convert.[ch] lex.yy.c convert.lex.c $BIN
    rm -rf "bandicoot-$VERSION"
}

dist()
{
    DIST_CFLAGS=$1

    clean
    compile $LIBS $PROGS
    echo
    link $PROGS
    echo

    d="bandicoot-$VERSION"
    mkdir -p $d

    cp LICENSE NOTICE README $BIN/bandicoot* $d
    if [ -d "$2" ]
    then
        echo "including examples directory $2"
        cp -r $2 $d
    fi

    a="$BIN/$d.tar.gz"
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
        $BIN/bandicoot start -c "test/test_defs.b" \
                             -d $BIN/volume \
                             -s $BIN/state \
                             -p $PORT &
        pid=$!

        # we need to finish the initialization
        # before we start storing the variables
        sleep 1

        for v in $ALL_VARS
        do
            curl -s http://127.0.0.1:$PORT/load_$v > /dev/null
            curl -s --data-binary @test/data/$v http://127.0.0.1:$PORT/store_$v
            curl -s http://127.0.0.1:$PORT/load_$v > /dev/null
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
                curl -s http://127.0.0.1:$PORT/load_$v -o /dev/null
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
                    http://127.0.0.1:$PORT/store_$v -o /dev/null
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
        cat test/progs/all.out | sort > bin/all_sorted.out
        diff -u bin/all_sorted.out bin/lang_test.out
        if [ "$?" != "0" ]
        then
            echo "---"
            echo "language test failed (inspect the differences above)"
        fi
        ;;
    todos)
        find . -regex '.*\.[chly]' -exec egrep -n -H -i 'fixme|todo|think' {} \;
        ;;
    dist)
        if [ "$2" != "-m32" ] && [ "$2" != "-m64" ]
        then
            echo "---"
            echo "expecting -m32|-m64 as the first parameter"
            exit 1
        fi

        dist "-Os $2" $3
        ;;
    *)
        echo "unknown command '$cmd', usage: ctl <command>"
        echo "    dist -m32|-m64 [examles/] - build a package for distribution"
        echo "    pack - compile and prepare for running tests"
        echo "    test - execute structured tests (prereq: pack)"
        echo "    perf - execute performance tests (prereq: pack)"
        echo "    clean - remove object files etc."
esac
