; sat: y = 0, x = 1
(set-logic ALL)
(set-option :produce-models true)
(declare-fun x () Int)
(declare-fun y () Int)

(assert (>= y 0))
(assert (= x (exp 2 y)))

(check-sat)
(get-model)
