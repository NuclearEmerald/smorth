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

: align 
    here aligned 
    here - allot 
;

: cell+ 8 + ;

: cells 8 * ;

: chars ;

: constant
  :
  postpone literal
  postpone ;
;

: loop immediate
    1 postpone literal
    postpone +loop
;

: variable 
    align here
    0 , 
    constant 
;

: [ immediate 
    0 state ! 
;

: ['] immediate 
    ' postpone literal 
;

: ] 1 state ! ;




: .( immediate
    41 parse type
;

: again immediate
    0 postpone literal
    postpone until
;