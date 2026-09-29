(set-logic ALL)
(declare-fun x () Int)
(declare-fun y () Int)

(assert (> (* x x) 4))
(assert (> (* y y) 4))
(assert (= (** (** x y) y) (** x (** y y))))

(check-sat)
