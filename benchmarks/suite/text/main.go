package main

import (
	"fmt"
	"os"
	"strconv"
	"strings"
)

var cities = []string{"Bangkok", "ChiangMai", "Phuket", "KhonKaen", "HatYai", "Pattaya", "UdonThani", "Korat", "HuaHin", "Ayutthaya"}

func main() {
	n := 1000000
	if len(os.Args) > 1 {
		if v, err := strconv.Atoi(os.Args[1]); err == nil {
			n = v
		}
	}
	parts := make([]string, 0)
	x := int64(7)
	for i := 0; i < n; i++ {
		x = (x*1103515245 + 12345) % 2147483648
		city := cities[(x/1000)%10]
		parts = append(parts, "user"+strconv.FormatInt(x%1000, 10)+","+city+","+strconv.FormatInt((x/10)%10000, 10))
	}
	data := strings.Join(parts, "\n")
	totals := map[string]int64{}
	users := map[string]int64{}
	grand := int64(0)
	lines := strings.Split(data, "\n")
	for _, line := range lines {
		f := strings.Split(line, ",")
		amount, _ := strconv.ParseInt(f[2], 10, 64)
		totals[f[1]] += amount
		users[f[0]]++
		grand += amount
	}
	best := ""
	bestTotal := int64(-1)
	for city, t := range totals {
		if t > bestTotal || (t == bestTotal && city < best) {
			best = city
			bestTotal = t
		}
	}
	fmt.Println(len(lines), len(users), best, bestTotal, grand)
}
