# BoomChecker API Backend

REST API backend for BoomChecker IoT system providing device registration and management with JWT-based authentication.

## API Endpoints

OpenAPI 3.0 documentation available at `/swagger/index.html` when server is running.

To regenerate documentation after code changes:

```bash
swag init -g main.go --output ./docs
```

## Architecture

Clean Architecture pattern with clear separation of concerns:

```
apps/api-backend/
├── main.go                      # Entry point, dependency injection
├── .env                         # Environment variables
├── internal/
│   ├── models/                  # GORM database models
│   ├── database/                # Database initialization
│   ├── validators/              # Input validation
│   ├── crypto/                  # Cryptography (AES-256-GCM, JWT)
│   ├── repositories/            # Data access layer
│   ├── services/                # Business logic
│   ├── handlers/                # HTTP handlers
│   └── middleware/              # HTTP middleware
└── scripts/
    └── generate_keys.go         # Encryption key generator
```

### Layers

1. **Models** - GORM models with hooks and helpers
2. **Database** - SQLite initialization, migrations, indexes
3. **Validators** - UUID, MAC, GPS, semantic versioning validation
4. **Crypto** - AES-256-GCM encryption and JWT operations
5. **Repositories** - Data access abstraction
6. **Services** - Business logic orchestration
7. **Handlers** - HTTP request/response handling
8. **Middleware** - Admin authentication (TODO)

## Setup

### Prerequisites

- Go 1.24+
- swag CLI for OpenAPI generation: `go install github.com/swaggo/swag/cmd/swag@latest`

### Installation

```bash
# Clone repository
git clone <repository-url>
cd boomchecker-monorepo/apps/api-backend

# Download dependencies
go mod download

# Generate OpenAPI documentation
swag init -g main.go --output ./docs

# Generate encryption key
go run scripts/generate_keys.go

# Create .env file
# Add JWT_ENCRYPTION_KEY from previous step

# Run server
go run main.go
```

Server runs on `http://localhost:8080`.

OpenAPI documentation: `http://localhost:8080/swagger/index.html`

### Environment Variables

Create `.env` file:

```env
JWT_ENCRYPTION_KEY=your-base64-encoded-key
DATABASE_PATH=./boomchecker.db
PORT=8080
GIN_MODE=release
```

## Testing

```bash
# Unit tests (validators, models)
go test ./internal/validators/... -v
go test ./internal/models/... -v

# Integration tests (repositories with in-memory SQLite)
go test ./internal/repositories/... -v

# All tests
go test ./... -v

# With coverage
go test ./... -cover
```

## Security

### JWT Secrets

JWT secrets are encrypted using AES-256-GCM before database storage:

- Master encryption key stored in `.env`
- Authenticated encryption (GCM mode)
- Random nonce per encryption
- 256-bit key and secret

### Registration Tokens

- Secure random generation (32 bytes via crypto/rand)
- Base64-URL encoding
- Time-limited with configurable expiration
- Usage-limited (default: 1 use)
- Optional MAC pre-authorization

### Validation

All inputs validated:
- UUID: RFC 4122 v4 format
- MAC: AA:BB:CC:DD:EE:FF (uppercase, normalized)
- GPS: Latitude (-90 to 90), Longitude (-180 to 180)
- Semantic Version: MAJOR.MINOR.PATCH
- Timestamps: UTC RFC3339

### Admin Authentication

Admin endpoints currently unprotected. Planned implementation in `internal/middleware/admin_auth.go`:
- Email-based JWT authentication
- POST /admin/auth/login
- Bearer token authorization
- 24h token validity
