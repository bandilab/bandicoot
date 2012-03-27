type test {r real, s string}

var t test;

type test_res {r real, s string, x real}

fn s_div_err() test_res
{
	return (extend x = s / r t);
}
