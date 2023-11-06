; sat: x = 2, y = 1, e2 = 4, e3 = 3
(set-logic ALL)
(set-option :produce-models true)
(declare-fun x () Int)
(declare-fun y () Int)
(declare-fun e2 () Int)
(declare-fun e3 () Int)

(assert (>= x 0))
(assert (>= y 0))
(assert (= e2 (exp 2 x)))
(assert (= e3 (exp 3 y)))
(assert (> e2 e3))

(check-sat)
(get-model)
