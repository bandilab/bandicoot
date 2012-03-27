type test {r real, s string}

var t test;

type test_res {r real, s string, x string}

fn s_sub_err() test_res
{
	return (extend x = s - r t);
}
