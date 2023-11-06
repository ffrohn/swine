; unsat
(set-logic ALL)
(declare-fun x () Int)
(declare-fun e2 () Int)
(declare-fun e3 () Int)

(assert (>= x 0))
(assert (= e2 (exp 2 x)))
(assert (= e3 (exp 3 x)))
(assert (> e2 e3))

(check-sat)
