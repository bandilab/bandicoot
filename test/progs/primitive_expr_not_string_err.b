type test {s string}

var t test;

fn s_not() test
{
	return (select !s t);
}
