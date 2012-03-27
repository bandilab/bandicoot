type T {i int, r real, l long}

fn test(t T) T
{
	var i_sel = (select i < (int l) (select i < (int r) (select i < (int i) t)));
	var r_sel = (select r < (real l) (select r < (real r) (select r < (real i) t)));
	var l_sel = (select l < (long l) (select l < (long r) (select l < (long i) t)));

	return (union (union i_sel r_sel) l_sel);
}
