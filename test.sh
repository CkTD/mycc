T=()
E=()

T+=("print 2;")
E+=("2")

T+=("print 3+2;")
E+=("5")

T+=("print 3-2-5;")
E+=("-4")

T+=("print 3-2*5;")
E+=("-7")

T+=("{int a; a = 33; print a;}")
E+=("33")

T+=("{int a; int b; a = b = 33; print a;}")
E+=("33")

T+=("{int a; int b; a = b = 33; print b;}")
E+=("33")

T+=("print 3>=2!=0;")
E+=("1")

T+=("print 3>2<1;")
E+=("0")

T+=("{print 3-(2-5);}")
E+=("6")

T+=("{print 3*(4+(3-1)*5); }")
E+=("42")

passed=0
failed=0

for i in ${!T[@]}; do
    prog=${T[$i]}
    expected=${E[$i]}

    # compile input
    ./mycc "${prog}" >temp.S 2>/dev/null
    if [ $? != "0" ]; then
        failed=$((failed + 1))
        echo "=== FAILED (compile) ==="
        echo prog ${prog}
        continue
    fi

    # build executable
    gcc temp.S -o temp.out 2>/dev/null
    if [ $? != "0" ]; then
        failed=$((failed + 1))
        echo "=== FAILED (assembly) ==="
        echo prog ${prog}
        continue
    fi

    # checkout output
    real=$(./temp.out)
    if [ "${real}" != "${expected}" ]; then
        failed=$((failed + 1))
        echo "=== FAILED (bad output) ==="
        echo prog ${prog}
        echo got ${real}
        echo expected ${expected}
        continue
    fi

    passed=$((passed + 1))
done

echo ${passed} PASSED, ${failed} FAILED
