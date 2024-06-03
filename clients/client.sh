#!/bin/sh

PORT=3000
ENDPOINT="http://localhost:${PORT}"
HEALTH_PATH="/healthz"
LOGIN_PATH="/login"
API_PATH="/api/v1"
USER_PATH="/user"
USERS_PATH="/users"
PASSWORD_PATH="/password"
PASSWORDS_PATH="/passwords"
#TOKEN=")NFuA?&y*poVF{viuSI5:aAt3.^?[TBK"

login() {
    TOKEN=$(curl -s \
        -H 'Content-Type: application/json' \
        -H 'Accept: application/json' \
        -d "{\"username\": \"bdowns\", \"password\": \"one4all\"}" "${ENDPOINT}${LOGIN_PATH}" | jq '.token')
    
    printf "Run:\n\texport HUSH_TOKEN=%s" "${TOKEN}"
}

get_password() {
    if [ -z "$1" ]; then
        echo "error: get_password requires an argument"
        exit 1
    fi
    name=$1

    if [ -z "${HUSH_TOKEN}" ]; then
        echo "error: no token set. Run $0 login"
        exit 1
    fi

    password=$(curl -s \
        -H 'Accept: application/json' \
        -H 'X-Hush-Auth: '"${HUSH_TOKEN}" \
        "${ENDPOINT}${API_PATH}${PASSWORD_PATH}/${name}")

    echo "${password}" | jq -r '.password'
}

{ # main
    while [ $# -gt 0 ];
    do
        opt="$1"
        shift
        case "$opt" in
            "login")
                login
            ;;
            "get")
                get_password "$1"
            ;;
            "set")

            ;;
        esac
    done

    exit 0
}
