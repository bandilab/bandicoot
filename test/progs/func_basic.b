rel point {
    x: real,
    y: real,
}

temperature: point;
unemployment: point;
currency: point;

fn get_pointless_results(): point
{
    return (temperature + currency * unemployment) - currency * temperature;
}

fn func_with_param(p: point): point
{
    return p * temperature + currency;
}
