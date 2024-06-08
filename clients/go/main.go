/*
Copyright © 2024 NAME HERE <EMAIL ADDRESS>
*/
package main

import "github.com/briandowns/hush/clients/go/cmd"

// loginReq
type loginReq struct {
	Username string `json:"username"`
	Password string `json:"password"`
}

// loginRes
type loginRes struct {
	Token string `json:"token"`
}

const (
	endpoint    = "http://localhost:8080"
	apiBasePath = "/api/v1"
	loginPath   = "/login"
)

func main() {
	cmd.Execute()
}
