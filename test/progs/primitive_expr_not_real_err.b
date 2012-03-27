type test {r real}

var t test;

fn s_not() test
{
	return (select !r t);
}
