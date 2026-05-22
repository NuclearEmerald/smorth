: ( immediate
    41 parse drop drop
;

: bl 32 ;

: parse-name bl parse ;

: ' 
    bl word find
    0= if 
    dup 1+ swap c@ 34 emit type 34 emit 10 emit
    bye
    then
;

: postpone immediate 
    ' compile, 
;

: align 
    here aligned 
    here - allot 
;

: cell+ 8 + ;

: cells 8 * ;

: char parse-name drop c@ ;

: char+ 1+ ;

: chars ;

: constant
  : postpone literal
  postpone ;
;

: count
    dup char+
    swap c@
;

: cr 10 emit ;

: loop immediate
    1 postpone literal
    postpone +loop
;

: variable 
    align here
    0 , 
    constant 
;

variable base
: decimal 10 base ! ;
: hex 16 base ! ;

: [ immediate 
    0 state ! 
;

: ['] immediate 
    ' postpone literal 
;

: [char] immediate
    char postpone literal
;

: ] 1 state ! ;

( non standard word )
: insert immediate
    postpone ['] ['] compile, compile,
;

: do immediate
    postpone (do)
    postpone begin
;

: ?do immediate
    insert 2dup
    postpone (do)
    insert = postpone if 
        insert leave
    postpone then
    postpone begin
;

: s" immediate
    [char] " parse 
    here 2dup 2>r
    swap dup 
    allot move 2r>
    state @ if
        postpone literal
        postpone literal
    else swap
    then
;

: space bl emit ;

: spaces 0 ?do space loop ;

: ." immediate
    state @ if
        postpone s"
        insert type
    else 
        postpone s" 
        swap over 
        type
        0 swap -
        allot
    then
;

: accept
    dup 0<> if
        2>r 0
        begin
            key dup 10 <> 
            over 13 <> =
        while
            dup 8 = if
                over 0> if 
                    dup emit bl emit emit 1-
                else drop
                then
            else
                over r@ > if drop
                else
                    dup emit
                    over 2r@ drop +
                    c! 1+
                then
            then
        repeat
        drop 2r>
    then 2drop
;




( extended words )

: .( immediate
    41 parse type
;

: again immediate
    0 postpone literal
    postpone until
;

: \ immediate
    source >in 
    ! drop
;