/*
Copyright © 2024 NAME HERE <EMAIL ADDRESS>
*/
package cmd

import (
	"bufio"
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"strings"
	"syscall"

	"github.com/spf13/cobra"
	"golang.org/x/term"
)

const (
	endpoint    = "http://localhost:8080"
	loginPath   = "/login"
	apiBasePath = "/api/v1"
	userPath    = "/user"
	usersPath   = "/users"
	keyPath     = "/user/key"
)

const clientDir = "~/.hush"

// loginReq
type loginReq struct {
	Username string `json:"username"`
	Password string `json:"password"`
}

// loginRes
type loginRes struct {
	Token string `json:"token"`
}

// keyRes
type keyRes struct {
	Key string `json:"key"`
}

// initCmd represents the init command
var initCmd = &cobra.Command{
	Use:   "init",
	Short: "A brief description of your command",
	Long: `A longer description that spans multiple lines and likely contains examples
and usage of using your command. For example:

Cobra is a CLI library for Go that empowers applications.
This application is a tool to generate the needed files
to quickly create a Cobra application.`,
	Run: func(cmd *cobra.Command, args []string) {
		var keyExists bool
		if _, err := os.Stat(clientDir); err != nil {
			if os.IsNotExist(err) {
				os.Mkdir(clientDir, 0755)
			} else {
				keyExists = true
			}
		}

		var token string

		if os.Getenv("HUSH_TOKEN") == "" {
			reader := bufio.NewReader(os.Stdin)
			fmt.Print("username: ")
			un, err := reader.ReadString('\n')
			if err != nil {
				fmt.Fprintln(os.Stderr, err.Error())
				os.Exit(1)
			}
			fmt.Print("password: ")

			b, err := term.ReadPassword(int(syscall.Stdin))
			if err != nil {
				fmt.Fprintln(os.Stderr, err.Error())
				os.Exit(1)
			}
			pw := string(b)

			lq := loginReq{
				Username: strings.TrimSpace(un),
				Password: pw,
			}
			jb, err := json.Marshal(&lq)
			if err != nil {
				fmt.Fprintln(os.Stderr, err.Error())
				os.Exit(1)
			}

			req, err := http.NewRequest(http.MethodPost, endpoint+loginPath, bytes.NewBuffer(jb))
			if err != nil {
				fmt.Fprintln(os.Stderr, err.Error())
				os.Exit(1)
			}

			req.Header = http.Header{
				"Content-Type": {"application/json"},
				"Accept":       {"application/json"},
			}

			res, err := hc.Do(req)
			if err != nil {
				fmt.Fprintln(os.Stderr, err.Error())
				os.Exit(1)
			}
			defer res.Body.Close()

			var lr loginRes
			if err := json.NewDecoder(res.Body).Decode(&lr); err != nil {
				fmt.Fprintln(os.Stderr, err.Error())
				os.Exit(1)
			}

			fmt.Printf("Run:\n\texport HUSH_TOKEN=%s \n", lr.Token)
			token = lr.Token
		} else {
			token = os.Getenv("HUSH_TOKEN")
		}

		if !keyExists {
			req, err := http.NewRequest(http.MethodGet, endpoint+apiBasePath+keyPath, nil)
			if err != nil {
				fmt.Fprintln(os.Stderr, err.Error())
				os.Exit(1)
			}

			req.Header = http.Header{
				"Accept":      {"application/json"},
				"X-Hush-Auth": {token},
			}

			res, err := hc.Do(req)
			if err != nil {
				fmt.Fprintln(os.Stderr, err.Error())
				os.Exit(1)
			}
			defer res.Body.Close()
			fmt.Println(res.StatusCode)
			var kr keyRes
			if err := json.NewDecoder(res.Body).Decode(&kr); err != nil {
				fmt.Fprintln(os.Stderr, err.Error())
				os.Exit(1)
			}
			fmt.Println("%+v\n", kr)
		}

	},
}

func init() {
	rootCmd.AddCommand(initCmd)

	// Here you will define your flags and configuration settings.

	// Cobra supports Persistent Flags which will work for this command
	// and all subcommands, e.g.:
	// initCmd.PersistentFlags().String("foo", "", "A help for foo")

	// Cobra supports local flags which will only run when this command
	// is called directly, e.g.:
	// initCmd.Flags().BoolP("toggle", "t", false, "Help message for toggle")
}
