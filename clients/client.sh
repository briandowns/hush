#!/bin/sh

PORT=8080
ENDPOINT="http://localhost:${PORT}"
HEALTH_PATH="/healthz"
LOGIN_PATH="/login"
API_PATH="/api/v1"
USER_PATH="/user"
USERS_PATH="/users"
PASSWORD_PATH="/password"
PASSWORDS_PATH="/passwords"

login() {
    TOKEN=$(curl -s \
        -H 'Content-Type: application/json' \
        -H 'Accept: application/json' \
        -d "{\"username\": \"bdowns\", \"password\": \"one4all\"}" "${ENDPOINT}${LOGIN_PATH}" | jq '.token')
    
    printf "Run:\n\texport HUSH_TOKEN=%s" "${TOKEN}\n"
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

    res=$(curl -s \
        -H 'Accept: application/json' \
        -H 'X-Hush-Auth: '"${HUSH_TOKEN}" \
        "${ENDPOINT}${API_PATH}${PASSWORD_PATH}/${name}")

    echo "$(echo "${res}" | jq -r '.username') $(echo "${res}" | jq -r '.password')"
}

set_password() {
    if [ -z "$1" ]; then
        echo "error: set_password requires name as first argument"
        exit 1
    fi
    name=$1

    if [ -z "$2" ]; then
        echo "error: set_password requires username as second argument"
        exit 1
    fi
    username=$2

    if [ -z "$3" ]; then
        echo "error: set_password requires password as third argument"
        exit 1
    fi
    password=$3

    curl -s -XPOST \
        -H 'Content-Type: application/json' \
        -H 'X-Hush-Auth: '"${HUSH_TOKEN}" \
        -d "{\"name\": \"${name}\",
             \"username\": \"${username}\",
             \"password\": \"${password}\"}" \
           "${ENDPOINT}${API_PATH}${PASSWORD_PATH}"
}

list_passwords() {
    if [ -z "${HUSH_TOKEN}" ]; then
        echo "error: no token set. Run $0 login"
        exit 1
    fi

    res=$(curl -s \
        -H 'Accept: application/json' \
        -H 'X-Hush-Auth: '"${HUSH_TOKEN}" \
        "${ENDPOINT}${API_PATH}${PASSWORDS_PATH}")

    echo "${res}" | jq -r '.passwords[].name' | while read -r name; do echo "${name}"; done
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
            "ls"|"list")
                list_passwords
            ;;
            "rm"|"remove")

            ;;
            "get")
                get_password "$1"
            ;;
            "set")
                set_password "$1" "$2" "$3"
            ;;
        esac
    done

    exit 0
}
