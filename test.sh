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

T+=("if(1==1) print 1;")
E+=("1")

T+=("if(1==2) {print 1;}")
E+=("")

T+=("if(0){print 1;} else {print 2;}")
E+=("2")

T+=("if(3) print 1; else print 2;")
E+=("1")

T+=("if(2>3){print 1;} else { if(2==3) print 2; else print 3;}")
E+=("3")

# dangling if:  https://en.wikipedia.org/wiki/Dangling_else
T+=("if(0) if (0) print 2; else print 3;")
E+=("")

T+=("while(0) print 1;")
E+=("")

T+=("{int a; a = 0; while(a<3)a=a+1; print a;}")
E+=("3")

T+=("do print 1;while(0);")
E+=("1")

T+=("{int i; i =0; do{i=(i+1)*3;} while(i<20); print i;}")
E+=("39")

T+=("{int i; int s; s=0;for(i=0;i<10;i=i+1)s=s+1;print s;}")
E+=("10")

T+=("{int i; int s; i=0;s=0;for(;i<10;i=i+1)s=s+1;print s;}")
E+=("10")

T+=("{int i; int s; i=0;s=0;for(;i<10;){s=s+1;i=i+1;}print s;}")
E+=("10")

T+=("{int a; int b; a = 0; b=0; while(a<5){a=a+1;if(a==2) continue; b=b+1;} print b;}")
E+=("4")

T+=("{int a; int b; a = 0; b=0; while(a<5){a=a+1;if(a==2) break; b=b+1;} print b;}")
E+=("1")

T+=("{int a; int b; a = 0; b=0; do{a=a+1; if(a==2) continue; b=b+1;} while(a<5); print b;}")
E+=("4")

T+=("{int a; int b; a = 0; b=0; do{a=a+1; if(a==2) break; b=b+1;} while(a<5); print b;}")
E+=("1")

# break should stop the inner iteration
T+=("{int a; int b; int c; a=0; b=0; while(a<5) {a=a+1; c=0; while(1){c=c+1; if(c==a) break;} b=b+c;} print b;}")
E+=("15")

T+=("{for(;;){break;}print 1;}")
E+=("1")

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
