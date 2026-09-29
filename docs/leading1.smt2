(set-logic ALL)
(declare-fun x () Int)
(declare-fun y () Int)

(assert (> (* x x) 4))
(assert (> (* y y) 4))
(assert (= (exp (exp x y) y) (exp x (exp y y))))

(check-sat)
