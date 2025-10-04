program fatoracao;
var n, i, fat: integer;
begin
  n := 5;
  fat := 1;
  i := 1;
  while i <= n do
  begin
    fat := fat * i;
    i := i + 1;
  end;
end.
