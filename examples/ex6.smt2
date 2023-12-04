(set-logic ALL)
(set-option :produce-models true)
(declare-fun x () Int)
(declare-fun y () Int)

(assert (> (ite (>= x 0) x (- x)) 2))
(assert (> (ite (>= y 0) y (- y)) 2))
(assert (= (exp (exp x y) y) (exp x (exp y y))))

(check-sat)
