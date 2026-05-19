: ( immediate
    41 parse drop drop
;

: bl 32 ;

: parse-name bl parse ;

: ' 
    parse-name  drop 
    find        drop 
;

: postpone immediate 
    ' compile, 
;

: ?dup dup if dup then ;

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
    state @ if
        34 parse
        swap 
        postpone literal
        postpone literal
    else 34 parse 
    then
;

: space bl emit ;

: spaces 0 ?do space loop ;

: ." immediate
    state @ if
        postpone s"
        insert type
    else postpone s" type
    then
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