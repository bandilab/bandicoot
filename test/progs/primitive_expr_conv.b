type T {i int, r real, l long}

fn test(t T) T
{
	var i_sel = (select i < (int l) (select i < (int r) (select i < (int i) t)));
	var r_sel = (select r < (real l) (select r < (real r) (select r < (real i) t)));
	var l_sel = (select l < (long l) (select l < (long r) (select l < (long i) t)));

	var x_i = select (i < (int (l + 10L))) t;
	var x_r = select (r < (real (i * -4))) t;
	var x_l = select (l < (long (i/1))) t;

	return (union (union i_sel r_sel) l_sel);
}
