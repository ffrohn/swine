(set-logic ALL)
(set-option :produce-models true)
(declare-fun x () Int)

(assert (> (exp 2 (+ x 1)) (exp 2 x)))

(check-sat)
(get-model)
