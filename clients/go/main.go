package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"time"
)

type loginReq struct {
	Username string `json:"username"`
	Password string `json:"password"`
}

type loginRes struct {
	Token string `json:"token"`
}

const (
	endpoint    = "http://localhost:8080"
	apiBasePath = "/api/v1"
	loginPath   = "/login"
)

func main() {
	hc := http.Client{
		Timeout: 10 * time.Second,
	}

	lq := loginReq{
		Username: "bdowns",
		Password: "one4all",
	}
	body, err := json.Marshal(lq)
	if err != nil {
		fmt.Fprintf(os.Stderr, "%s", err.Error())
		os.Exit(1)
	}

	req, err := http.NewRequest(http.MethodPost, endpoint+loginPath, bytes.NewBuffer(body))
	if err != nil {
		fmt.Fprintf(os.Stderr, "%s", err.Error())
		os.Exit(1)
	}
	req.Header.Add("Content-Type", "application/json")
	req.Header.Add("Accept", "application/json")

	res, err := hc.Do(req)
	if err != nil {
		fmt.Fprintf(os.Stderr, "%s", err.Error())
		os.Exit(1)
	}
	defer res.Body.Close()

	if res.StatusCode > 200 {
		fmt.Fprintf(os.Stderr, "%s\n", res.Status)
		os.Exit(1)
	}

	var lr loginRes
	if err := json.NewDecoder(res.Body).Decode(&lr); err != nil {
		fmt.Fprintf(os.Stderr, "%s", err.Error())
		os.Exit(1)
	}

	fmt.Printf("%#v\n", lr)

	os.Exit(0)
}
